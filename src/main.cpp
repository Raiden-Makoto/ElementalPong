// Basic 960x720 Breakout clone using raylib and C++.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <queue>
#include <string>
#include <vector>
#include <sstream>

struct Paddle {
    Rectangle rect{};
    float speed{640.0f}; // pixels per second
    Color color{LIGHTGRAY};
    int colorIndex{0};
};

struct Ball {
    Vector2 position{};
    Vector2 velocity{};
    float radius{10.0f};
    float speed{420.0f};
    bool inPlay{false};
    Color color{WHITE};
    int colorIndex{0};
    bool overloaded{false};
    bool superconduct{false};
    bool frozen{false};
    bool freezeReady{false};
    float freezeTimer{0.0f};
    Vector2 storedVelocity{};
    bool vaporizeReady{false};
};

struct Brick {
    Rectangle rect{};
    bool active{true};
    Color baseColor{WHITE};
    Color color{WHITE};
    int row{0};
    int col{0};
    int colorIndex{-1};
    int hitPoints{2};
    bool cracked{false};
    bool frozen{false};
};

constexpr int ScreenWidth = 960;
constexpr int ScreenHeight = 720;
constexpr int BrickCols = 12;
constexpr int BrickRows = 7;
constexpr float BrickSpacing = 8.0f;
constexpr float BrickHeight = 28.0f;
constexpr float BrickTopOffset = 100.0f;

constexpr Color BrickPalette[] = {
    {255, 102, 0, 255},   // orange-red
    {0, 112, 221, 255},   // blue
    {0, 191, 165, 255},   // teal-green
    {196, 120, 255, 255}, // light purple
    {173, 216, 230, 255}, // light blue/white
};
constexpr int BrickPaletteCount = sizeof(BrickPalette) / sizeof(Color);
constexpr int ColorIndexRed = 0;
constexpr int ColorIndexBlue = 1;
constexpr int ColorIndexPurple = 3;
constexpr int ColorIndexLightBlue = 4;
constexpr int ColorIndexGreen = 2;
constexpr float OverloadAoEDelay = 0.18f;
constexpr float SurgeChainStepDelay = 0.08f;

Sound gBounceSound{};
Sound gGameOverSound{};
bool gAudioReady = false;
std::vector<std::string> gHelpWrappedLines;
float gHelpWrapWidth = 0.0f;

static const char* HelpLines[] = {
    "Elemental Breakout - How to Play",
    "",
    "Controls",
    "  - Left / Right or A / D: Move paddle",
    "  - Space: Launch ball",
    "  - Enter: Start a new wave / continue",
    "  - Q: Forfeit run",
    "  - 1-5: Change paddle element",
    "  - P: Pause",
    "",
    "Elemental Reactions",
    "  - Overloaded (Purple + Red paddle): Ball supercharges, next brick causes an AoE explosion.",
    "  - Swirl (Green ball + non-green brick): Spreads the new element to nearby bricks.",
    "  - Freeze (Blue + Light Blue paddle): Ball freezes on paddle, next brick freezes connected cluster.",
    "  - Melt (Red ball + Frozen brick): Thaws the brick back to yellow.",
    "  - Vaporize (Blue ball + Red brick): Instantly destroys the brick.",
    "  - Liquefy (Light Blue ball + Red brick): Converts the brick to blue.",
    "  - Superconduct (Purple + Light Blue paddle): Ball phases through bricks.",
    "  - Surge (Purple ball + Blue brick, or Blue ball + Purple brick): Lightning arc clears diagonal lines.",
    "  - Infuse (Any non-green ball + Green brick): Converts adjacent green bricks to the ball's element.",
    "  - Frozen clusters shattered by other colors chain-break neighboring frozen bricks.",
    "",
    "Progression",
    "  - Clearing all bricks spawns a fresh wave and increases ball speed by 15%.",
    "  - You have one life; falling off the screen ends the run.",
    "",
    "Press Enter or Space to begin!",
};
constexpr int HelpLineCount = sizeof(HelpLines) / sizeof(HelpLines[0]);

Color LightenColor(Color color, float factor) {
    auto lightenChannel = [factor](unsigned char channel) -> unsigned char {
        int value = static_cast<int>(channel + (255 - channel) * factor);
        value = std::clamp(value, 0, 255);
        return static_cast<unsigned char>(value);
    };

    return Color{
        lightenChannel(color.r),
        lightenChannel(color.g),
        lightenChannel(color.b),
        color.a
    };
}

Color DarkenColor(Color color, float factor) {
    auto darkenChannel = [factor](unsigned char channel) -> unsigned char {
        int value = static_cast<int>(channel * (1.0f - factor));
        value = std::clamp(value, 0, 255);
        return static_cast<unsigned char>(value);
    };

    return Color{
        darkenChannel(color.r),
        darkenChannel(color.g),
        darkenChannel(color.b),
        color.a
    };
}

Brick* GetBrickAt(std::vector<Brick>& bricks, int row, int col) {
    for (Brick& brick : bricks) {
        if (brick.row == row && brick.col == col) {
            return &brick;
        }
    }
    return nullptr;
}

int FreezeConnectedBricks(std::vector<Brick>& bricks, int startRow, int startCol, int targetColorIndex) {
    if (targetColorIndex < 0) {
        return 0;
    }

    int frozenCount = 0;
    bool visited[BrickRows][BrickCols] = {};
    std::queue<std::pair<int, int>> toVisit;
    toVisit.emplace(startRow, startCol);

    const std::pair<int, int> directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!toVisit.empty()) {
        auto [row, col] = toVisit.front();
        toVisit.pop();

        if (row < 0 || row >= BrickRows || col < 0 || col >= BrickCols) {
            continue;
        }
        if (visited[row][col]) {
            continue;
        }
        visited[row][col] = true;

        Brick* brick = GetBrickAt(bricks, row, col);
        if (brick == nullptr || !brick->active || brick->colorIndex != targetColorIndex) {
            continue;
        }

        brick->baseColor = WHITE;
        brick->color = WHITE;
        brick->colorIndex = -1;
        brick->cracked = false;
        brick->hitPoints = 1;
        brick->frozen = true;
        frozenCount += 1;

        for (const auto& dir : directions) {
            toVisit.emplace(row + dir.first, col + dir.second);
        }
    }

    return frozenCount;
}

void DestroyBrick(Brick& brick);

int ShatterFrozenNeighbors(std::vector<Brick>& bricks, int startRow, int startCol) {
    int shattered = 0;
    bool visited[BrickRows][BrickCols] = {};
    std::queue<std::pair<int, int>> toVisit;

    const std::pair<int, int> directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& dir : directions) {
        toVisit.emplace(startRow + dir.first, startCol + dir.second);
    }

    while (!toVisit.empty()) {
        auto [row, col] = toVisit.front();
        toVisit.pop();

        if (row < 0 || row >= BrickRows || col < 0 || col >= BrickCols) {
            continue;
        }
        if (visited[row][col]) {
            continue;
        }
        visited[row][col] = true;

        Brick* brick = GetBrickAt(bricks, row, col);
        if (brick == nullptr || !brick->active || !brick->frozen) {
            continue;
        }

        brick->baseColor = WHITE;
        brick->color = WHITE;
        brick->colorIndex = -1;
        DestroyBrick(*brick);
        shattered += 1;

        for (const auto& dir : directions) {
            toVisit.emplace(row + dir.first, col + dir.second);
        }
    }

    return shattered;
}

void DestroyBrick(Brick& brick) {
    brick.active = false;
    brick.hitPoints = 0;
    brick.cracked = false;
    brick.frozen = false;
    brick.color = brick.baseColor;
    brick.colorIndex = -1;
}

int InfuseAdjacentBricks(std::vector<Brick>& bricks, int startRow, int startCol, int newColorIndex, Color newColor) {
    bool visited[BrickRows][BrickCols] = {};
    std::queue<std::pair<int, int>> toVisit;
    toVisit.emplace(startRow, startCol);

    const std::pair<int, int> directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int infused = 0;

    while (!toVisit.empty()) {
        auto [row, col] = toVisit.front();
        toVisit.pop();

        if (row < 0 || row >= BrickRows || col < 0 || col >= BrickCols) {
            continue;
        }
        if (visited[row][col]) {
            continue;
        }
        visited[row][col] = true;

        Brick* brick = GetBrickAt(bricks, row, col);
        if (brick == nullptr || !brick->active || brick->colorIndex != ColorIndexGreen) {
            continue;
        }

        brick->baseColor = newColor;
        brick->color = newColor;
        brick->colorIndex = newColorIndex;
        brick->cracked = false;
        brick->hitPoints = std::max(brick->hitPoints, 2);
        brick->frozen = false;
        infused += 1;

        for (const auto& dir : directions) {
            toVisit.emplace(row + dir.first, col + dir.second);
        }
    }

    return infused;
}

struct ReactionMessage {
    std::string text{};
    Color color{WHITE};
    float timer{0.0f};
    bool active{false};
};

enum class ReactionKind {
    OverloadAoE,
    SurgeChain,
};

struct ReactionEvent {
    int row;
    int col;
    float timer;
    ReactionKind kind;
};

void ShowReactionMessage(ReactionMessage& message, const std::string& text, Color color, float duration = 1.2f) {
    message.text = text;
    message.color = color;
    message.timer = duration;
    message.active = true;
}

void UpdateReactionMessage(ReactionMessage& message, float dt) {
    if (!message.active) {
        return;
    }
    message.timer -= dt;
    if (message.timer <= 0.0f) {
        message.active = false;
    }
}

void ScheduleSurgeChain(std::vector<ReactionEvent>& events, std::vector<Brick>& bricks, int startRow, int startCol) {
    const std::pair<int, int> directions[] = {{1, 1}, {-1, -1}, {1, -1}, {-1, 1}};

    for (const auto& dir : directions) {
        int row = startRow + dir.first;
        int col = startCol + dir.second;
        int distance = 1;
        while (row >= 0 && row < BrickRows && col >= 0 && col < BrickCols) {
            Brick* target = GetBrickAt(bricks, row, col);
            if (target != nullptr && target->active) {
                events.push_back(ReactionEvent{
                    .row = row,
                    .col = col,
                    .timer = SurgeChainStepDelay * static_cast<float>(distance),
                    .kind = ReactionKind::SurgeChain,
                });
            }
            row += dir.first;
            col += dir.second;
            distance += 1;
        }
    }
}

Vector2 Normalize(Vector2 v) {
    float lengthSq = v.x * v.x + v.y * v.y;
    if (lengthSq <= 0.0001f) {
        return {0.0f, 0.0f};
    }
    float invLength = 1.0f / std::sqrt(lengthSq);
    return {v.x * invLength, v.y * invLength};
}

Vector2 Scale(Vector2 v, float s) {
    return {v.x * s, v.y * s};
}

std::vector<Brick> CreateBricks() {
    std::vector<Brick> bricks;
    bricks.reserve(BrickCols * BrickRows);

    float totalSpacingX = (BrickCols + 1) * BrickSpacing;
    float availableWidth = ScreenWidth - totalSpacingX;
    float brickWidth = availableWidth / BrickCols;
    for (int row = 0; row < BrickRows; ++row) {
        int col = 0;
        while (col < BrickCols) {
            int remaining = BrickCols - col;
            int chunkSize = GetRandomValue(3, 6);
            if (chunkSize > remaining) {
                chunkSize = remaining;
            }

            int colorIdx = GetRandomValue(0, BrickPaletteCount - 1);
            Color chunkColor = BrickPalette[colorIdx];

            bool chunkNeutral = GetRandomValue(0, 99) < 15;
            Color neutralColor = {255, 221, 0, 255}; // yellow
            if (chunkNeutral) {
                colorIdx = -1;
                chunkColor = neutralColor;
            }

            for (int i = 0; i < chunkSize; ++i) {
                int currentCol = col + i;
                float x = BrickSpacing + currentCol * (brickWidth + BrickSpacing);
                float y = BrickTopOffset + row * (BrickHeight + BrickSpacing);

                bool hasGap = GetRandomValue(0, 99) < 28; // chance to skip individual brick

                if (hasGap) {
                    continue;
                }

                bricks.push_back(Brick{
                    .rect = {x, y, brickWidth, BrickHeight},
                    .active = true,
                    .baseColor = chunkColor,
                    .color = chunkColor,
                    .row = row,
                    .col = currentCol,
                    .colorIndex = colorIdx,
                    .hitPoints = 2,
                    .cracked = false,
                    .frozen = false,
                });
            }

            col += chunkSize;
        }
    }

    return bricks;
}

std::vector<std::string> WrapHelpLines(float maxWidth, int fontSize) {
    std::vector<std::string> wrapped;
    wrapped.reserve(HelpLineCount * 2);

    for (int i = 0; i < HelpLineCount; ++i) {
        const std::string line = HelpLines[i];
        if (line.empty()) {
            wrapped.emplace_back("");
            continue;
        }

        std::istringstream iss(line);
        std::string word;
        std::string current;
        while (iss >> word) {
            std::string candidate = current.empty() ? word : current + " " + word;
            if (!current.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth) {
                wrapped.push_back(current);
                current = word;
            } else {
                current = candidate;
            }
        }

        if (!current.empty()) {
            wrapped.push_back(current);
        }
    }

    return wrapped;
}

int ApplyOverloadedAoE(std::vector<Brick>& bricks, int centerRow, int centerCol) {
    int removed = 0;
    for (Brick& brick : bricks) {
        if (!brick.active) {
            continue;
        }
        int dRow = std::abs(brick.row - centerRow);
        int dCol = std::abs(brick.col - centerCol);
        if (dRow <= 1 && dCol <= 1) {
            DestroyBrick(brick);
            removed += 1;
        }
    }
    return removed;
}

int CountActiveBricks(const std::vector<Brick>& bricks) {
    int active = 0;
    for (const Brick& brick : bricks) {
        if (brick.active) {
            active += 1;
        }
    }
    return active;
}

int ResolveReactionEvents(float dt, std::vector<ReactionEvent>& events, std::vector<Brick>& bricks) {
    int removed = 0;
    for (ReactionEvent& event : events) {
        event.timer -= dt;
    }

    auto it = events.begin();
    while (it != events.end()) {
        if (it->timer <= 0.0f) {
            if (it->kind == ReactionKind::OverloadAoE) {
                removed += ApplyOverloadedAoE(bricks, it->row, it->col);
            } else if (it->kind == ReactionKind::SurgeChain) {
                Brick* target = GetBrickAt(bricks, it->row, it->col);
                if (target != nullptr && target->active) {
                    DestroyBrick(*target);
                    removed += 1;
                }
            }
            it = events.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

void ResetBallOnPaddle(Ball& ball, const Paddle& paddle) {
    ball.inPlay = false;
    ball.overloaded = false;
    ball.superconduct = false;
    ball.frozen = false;
    ball.freezeReady = false;
    ball.freezeTimer = 0.0f;
    ball.storedVelocity = {};
    ball.vaporizeReady = false;
    ball.colorIndex = -1;
    ball.color = WHITE;
    ball.position = {
        paddle.rect.x + paddle.rect.width * 0.5f,
        paddle.rect.y - ball.radius - 1.0f,
    };
    ball.velocity = {0.0f, 0.0f};
}

void LaunchBall(Ball& ball) {
    if (ball.inPlay) {
        return;
    }
    float direction = GetRandomValue(0, 1) == 0 ? -1.0f : 1.0f;
    Vector2 initialDir{direction * 0.6f, -1.0f};
    initialDir = Normalize(initialDir);
    ball.velocity = Scale(initialDir, ball.speed);
    ball.inPlay = true;
}

void UpdatePaddle(Paddle& paddle, float dt) {
    float dx = 0.0f;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        dx -= paddle.speed * dt;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        dx += paddle.speed * dt;
    }

    paddle.rect.x += dx;
    if (paddle.rect.x < 0.0f) {
        paddle.rect.x = 0.0f;
    }
    if (paddle.rect.x + paddle.rect.width > ScreenWidth) {
        paddle.rect.x = ScreenWidth - paddle.rect.width;
    }
}

void HandleBallWallCollisions(Ball& ball) {
    bool bounced = false;
    if (ball.position.x - ball.radius <= 0.0f) {
        ball.position.x = ball.radius;
        ball.velocity.x *= -1.0f;
        bounced = true;
    } else if (ball.position.x + ball.radius >= ScreenWidth) {
        ball.position.x = ScreenWidth - ball.radius;
        ball.velocity.x *= -1.0f;
        bounced = true;
    }

    if (ball.position.y - ball.radius <= 0.0f) {
        ball.position.y = ball.radius;
        ball.velocity.y *= -1.0f;
        bounced = true;
    }

    if (bounced && gAudioReady) {
        PlaySound(gBounceSound);
    }
}

bool HandleBallPaddleCollision(Ball& ball, const Paddle& paddle) {
    if (!ball.inPlay) {
        return false;
    }

    if (!CheckCollisionCircleRec(ball.position, ball.radius, paddle.rect)) {
        return false;
    }

    ball.position.y = paddle.rect.y - ball.radius - 1.0f;
    float paddleCenter = paddle.rect.x + paddle.rect.width * 0.5f;
    float relative = (ball.position.x - paddleCenter) / (paddle.rect.width * 0.5f);
    relative = std::clamp(relative, -1.0f, 1.0f);

    Vector2 direction{relative, -1.0f};
    direction = Normalize(direction);
    ball.velocity = Scale(direction, ball.speed);

    bool overloadedTrigger = (ball.colorIndex == ColorIndexPurple && paddle.colorIndex == ColorIndexRed) ||
                             (ball.colorIndex == ColorIndexRed && paddle.colorIndex == ColorIndexPurple);
    bool superconductTrigger = (ball.colorIndex == ColorIndexPurple && paddle.colorIndex == ColorIndexLightBlue) ||
                               (ball.colorIndex == ColorIndexLightBlue && paddle.colorIndex == ColorIndexPurple);
    bool freezeTrigger = (ball.colorIndex == ColorIndexBlue && paddle.colorIndex == ColorIndexLightBlue) ||
                         (ball.colorIndex == ColorIndexLightBlue && paddle.colorIndex == ColorIndexBlue);
    if (paddle.colorIndex >= 0 && paddle.colorIndex < BrickPaletteCount) {
        ball.colorIndex = paddle.colorIndex;
        ball.color = BrickPalette[ball.colorIndex];
    } else {
        ball.colorIndex = -1;
        ball.color = WHITE;
    }
    ball.overloaded = overloadedTrigger;
    ball.superconduct = superconductTrigger;
    if (freezeTrigger) {
        ball.freezeReady = true;
        ball.frozen = true;
        ball.freezeTimer = 2.0f;
        ball.storedVelocity = ball.velocity;
        ball.velocity = {0.0f, 0.0f};
    } else {
        ball.freezeReady = false;
        ball.frozen = false;
        ball.freezeTimer = 0.0f;
        ball.storedVelocity = {};
    }
    ball.vaporizeReady = false;
    if (gAudioReady) {
        PlaySound(gBounceSound);
    }
    return true;
}

int HandleBallBrickCollision(Ball& ball, std::vector<Brick>& bricks, Vector2 previousPosition, std::vector<ReactionEvent>& reactionEvents, ReactionMessage& reactionMessage) {
    if (!ball.inPlay) {
        return 0;
    }

    int bricksBroken = 0;
    for (Brick& brick : bricks) {
        if (!brick.active) {
            continue;
        }
        if (!CheckCollisionCircleRec(ball.position, ball.radius, brick.rect)) {
            continue;
        }

        int freezeColorIndex = brick.colorIndex;

        if (ball.freezeReady) {
            if (freezeColorIndex != -1) {
                int frozenBricks = FreezeConnectedBricks(bricks, brick.row, brick.col, freezeColorIndex);
                if (frozenBricks > 0) {
                    ShowReactionMessage(reactionMessage, "Freeze!", BrickPalette[ColorIndexLightBlue]);
                }
            }
            ball.freezeReady = false;
        }

        bool brickBounced = false;

        if (!ball.superconduct) {
            bool collidedFromLeft = previousPosition.x + ball.radius <= brick.rect.x;
            bool collidedFromRight = previousPosition.x - ball.radius >= brick.rect.x + brick.rect.width;
            bool collidedFromTop = previousPosition.y + ball.radius <= brick.rect.y;
            bool collidedFromBottom = previousPosition.y - ball.radius >= brick.rect.y + brick.rect.height;

            bool resolved = false;

            if (collidedFromLeft || collidedFromRight) {
                ball.velocity.x *= -1.0f;
                if (collidedFromLeft) {
                    ball.position.x = brick.rect.x - ball.radius;
                } else {
                    ball.position.x = brick.rect.x + brick.rect.width + ball.radius;
                }
                resolved = true;
                brickBounced = true;
            }

            if (!resolved && (collidedFromTop || collidedFromBottom)) {
                ball.velocity.y *= -1.0f;
                if (collidedFromTop) {
                    ball.position.y = brick.rect.y - ball.radius;
                } else {
                    ball.position.y = brick.rect.y + brick.rect.height + ball.radius;
                }
                resolved = true;
                brickBounced = true;
            }

            if (!resolved) {
                float brickCenterX = brick.rect.x + brick.rect.width * 0.5f;
                float brickCenterY = brick.rect.y + brick.rect.height * 0.5f;
                float diffX = ball.position.x - brickCenterX;
                float diffY = ball.position.y - brickCenterY;

                if (std::abs(diffX) > std::abs(diffY)) {
                    ball.velocity.x *= -1.0f;
                    if (diffX > 0.0f) {
                        ball.position.x = brick.rect.x + brick.rect.width + ball.radius;
                    } else {
                        ball.position.x = brick.rect.x - ball.radius;
                    }
                    brickBounced = true;
                } else {
                    ball.velocity.y *= -1.0f;
                    if (diffY > 0.0f) {
                        ball.position.y = brick.rect.y + brick.rect.height + ball.radius;
                    } else {
                        ball.position.y = brick.rect.y - ball.radius;
                    }
                    brickBounced = true;
                }
            }
        }

        if (brickBounced && gAudioReady) {
            PlaySound(gBounceSound);
        }

        bool triggeredSwirl = (ball.colorIndex == ColorIndexGreen) &&
                              (brick.colorIndex != ColorIndexGreen) &&
                              (brick.colorIndex != -1);

        bool overloadTriggered = ball.overloaded;
        bool instantBreak = triggeredSwirl || overloadTriggered;
        bool destroyedThisHit = false;
        bool shatterFrozenCluster = false;

        bool vaporizeTriggered = false;
        bool infuseTriggered = false;
        bool meltTriggered = false;
        bool liquefyTriggered = false;
        bool surgeTriggered = false;
        if (ball.colorIndex == ColorIndexBlue && brick.colorIndex == ColorIndexRed) {
            instantBreak = true;
            vaporizeTriggered = true;
            ShowReactionMessage(reactionMessage, "Vaporize!", BrickPalette[ColorIndexBlue]);
        } else if (ball.colorIndex == ColorIndexLightBlue && brick.colorIndex == ColorIndexRed) {
            liquefyTriggered = true;
            ShowReactionMessage(reactionMessage, "Liquefy!", BrickPalette[ColorIndexBlue]);
        } else if ((ball.colorIndex == ColorIndexPurple && brick.colorIndex == ColorIndexBlue) ||
                   (ball.colorIndex == ColorIndexBlue && brick.colorIndex == ColorIndexPurple)) {
            surgeTriggered = true;
            instantBreak = true;
            ShowReactionMessage(reactionMessage, "Surge!", BrickPalette[ColorIndexPurple]);
        } else if (ball.colorIndex != ColorIndexGreen && brick.colorIndex == ColorIndexGreen) {
            int infused = InfuseAdjacentBricks(bricks, brick.row, brick.col, ball.colorIndex, ball.color);
            if (infused > 0) {
                infuseTriggered = true;
                ShowReactionMessage(reactionMessage, "Infuse!", BrickPalette[ColorIndexGreen]);
            }
        }

        if (brick.frozen) {
            if (ball.colorIndex == ColorIndexRed) {
                brick.frozen = false;
                brick.baseColor = {255, 221, 0, 255};
                brick.color = brick.baseColor;
                brick.colorIndex = -1;
                brick.hitPoints = 1;
                brick.cracked = false;
                meltTriggered = true;
                ShowReactionMessage(reactionMessage, "Melt!", ORANGE);
            } else {
                instantBreak = true;
                shatterFrozenCluster = true;
            }
        }

        if (instantBreak) {
            DestroyBrick(brick);
            destroyedThisHit = true;
        } else if (liquefyTriggered) {
            brick.baseColor = BrickPalette[ColorIndexBlue];
            brick.color = brick.baseColor;
            brick.colorIndex = ColorIndexBlue;
            brick.cracked = false;
            brick.hitPoints = std::max(brick.hitPoints, 2);
        } else if (infuseTriggered) {
            brick.baseColor = ball.color;
            brick.color = ball.color;
            brick.colorIndex = ball.colorIndex;
        } else {
            brick.hitPoints -= 1;
            if (brick.hitPoints <= 0) {
                DestroyBrick(brick);
                destroyedThisHit = true;
            } else {
                brick.cracked = true;
                brick.color = DarkenColor(brick.baseColor, 0.35f);
            }
        }

        if (triggeredSwirl) {
            reactionEvents.push_back(ReactionEvent{
                .row = brick.row,
                .col = brick.col,
                .timer = OverloadAoEDelay,
                .kind = ReactionKind::OverloadAoE,
            });
            ShowReactionMessage(reactionMessage, "Swirl!", BrickPalette[ColorIndexGreen]);
        }

        if (overloadTriggered) {
            reactionEvents.push_back(ReactionEvent{
                .row = brick.row,
                .col = brick.col,
                .timer = OverloadAoEDelay,
                .kind = ReactionKind::OverloadAoE,
            });
            ShowReactionMessage(reactionMessage, "Overloaded!", BrickPalette[ColorIndexRed]);
            ball.overloaded = false;
        }

        if (destroyedThisHit) {
            bricksBroken += 1;
            if (shatterFrozenCluster) {
                int shattered = ShatterFrozenNeighbors(bricks, brick.row, brick.col);
                bricksBroken += shattered;
            }
            if (vaporizeTriggered) {
                // already handled by destroyBrick which resets color index
            }
            if (surgeTriggered) {
                ScheduleSurgeChain(reactionEvents, bricks, brick.row, brick.col);
            }
        }

        break;
    }
    return bricksBroken;
}

void HandlePaddleColorInput(Paddle& paddle) {
    const int keys[] = {
        KEY_ONE,
        KEY_TWO,
        KEY_THREE,
        KEY_FOUR,
        KEY_FIVE,
    };

    for (int i = 0; i < BrickPaletteCount && i < static_cast<int>(sizeof(keys) / sizeof(int)); ++i) {
        if (IsKeyPressed(keys[i])) {
            paddle.colorIndex = i;
            paddle.color = BrickPalette[i];
            break;
        }
    }
}

int main() {
    SetRandomSeed(static_cast<unsigned int>(std::time(nullptr)));
    InitWindow(ScreenWidth, ScreenHeight, "Elemental Breakout");
    InitAudioDevice();
    gAudioReady = IsAudioDeviceReady();
    if (gAudioReady) {
        gBounceSound = LoadSound("sounds/bounce.mp3");
        gGameOverSound = LoadSound("sounds/gameover.mp3");
    }
    SetTargetFPS(60);

    Paddle paddle{{ScreenWidth / 2.0f - 60.0f, ScreenHeight - 80.0f, 120.0f, 20.0f}};
    paddle.colorIndex = ColorIndexPurple;
    paddle.color = BrickPalette[paddle.colorIndex];

    Ball ball{};
    ball.colorIndex = -1;
    ball.color = WHITE;
    ResetBallOnPaddle(ball, paddle);

    std::vector<Brick> bricks = CreateBricks();
    std::vector<ReactionEvent> reactionEvents;
    ReactionMessage reactionMessage;

    int score = 0;
    int lives = 1;
    bool gameWon = false;
    bool gameOver = false;
    bool paused = false;
    bool gameOverSoundPlayed = false;
    bool showHowTo = true;
    float howToScroll = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (showHowTo) {
            float wheel = GetMouseWheelMove();
            howToScroll += wheel * -48.0f;
            if (IsKeyDown(KEY_DOWN)) {
                howToScroll += 180.0f * dt;
            }
            if (IsKeyDown(KEY_UP)) {
                howToScroll -= 180.0f * dt;
            }
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Elemental Breakout", ScreenWidth / 2 - MeasureText("Elemental Breakout", 48) / 2, 40, 48, WHITE);

            Rectangle panelRect{60.0f, 80.0f, static_cast<float>(ScreenWidth - 120), static_cast<float>(ScreenHeight - 160)};
            DrawRectangleRounded(panelRect, 0.1f, 8, Fade(BLACK, 0.85f));
            DrawRectangleRoundedLines(panelRect, 0.1f, 8, Fade(WHITE, 0.4f));

            const int fontSize = 22;
            const int lineSpacing = 28;
            float usableWidth = panelRect.width - 80.0f;
            if (gHelpWrappedLines.empty() || std::fabs(gHelpWrapWidth - usableWidth) > 1.0f) {
                gHelpWrappedLines = WrapHelpLines(usableWidth, fontSize);
                gHelpWrapWidth = usableWidth;
            }

            int availableHeight = static_cast<int>(panelRect.height) - 80;
            int totalHeight = static_cast<int>(gHelpWrappedLines.size()) * lineSpacing;
            if (totalHeight < availableHeight) {
                howToScroll = 0.0f;
            } else {
                float maxScroll = static_cast<float>(totalHeight - availableHeight);
                if (howToScroll < 0.0f) howToScroll = 0.0f;
                if (howToScroll > maxScroll) howToScroll = maxScroll;
            }

            int baseY = static_cast<int>(panelRect.y) + 40;
            for (size_t i = 0; i < gHelpWrappedLines.size(); ++i) {
                int drawY = baseY + i * lineSpacing - static_cast<int>(howToScroll);
                if (drawY < panelRect.y + 30 || drawY > panelRect.y + panelRect.height - 50) {
                    continue;
                }
                Color lineColor = (i == 0) ? YELLOW : LIGHTGRAY;
                DrawText(gHelpWrappedLines[i].c_str(), static_cast<int>(panelRect.x) + 40, drawY, fontSize, lineColor);
            }

            int hintY = static_cast<int>(panelRect.y + panelRect.height) + 20;
            const char* hintScroll = "Mouse wheel / Arrow keys to scroll";
            const char* hintStart = "Press Enter or Space to start";
            DrawText(hintScroll, ScreenWidth / 2 - MeasureText(hintScroll, 20) / 2, hintY, 20, GRAY);
            DrawText(hintStart, ScreenWidth / 2 - MeasureText(hintStart, 20) / 2, hintY + 28, 20, GRAY);

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                showHowTo = false;
                howToScroll = 0.0f;
                ResetBallOnPaddle(ball, paddle);
            }

            EndDrawing();
            continue;
        }

        if (!gameOver && !gameWon && IsKeyPressed(KEY_P)) {
            paused = !paused;
        }

        if (!paused && !gameOver && !gameWon) {
            UpdatePaddle(paddle, dt);
            HandlePaddleColorInput(paddle);
        }

        if (!paused) {
            UpdateReactionMessage(reactionMessage, dt);
            if (ball.inPlay && ball.frozen) {
                ball.freezeTimer -= dt;
                ball.position.x = paddle.rect.x + paddle.rect.width * 0.5f;
                ball.position.y = paddle.rect.y - ball.radius - 1.0f;
                if (ball.freezeTimer <= 0.0f) {
                    ball.frozen = false;
                    float storedSpeed = std::sqrt(ball.storedVelocity.x * ball.storedVelocity.x + ball.storedVelocity.y * ball.storedVelocity.y);
                    if (storedSpeed <= 0.001f) {
                        ball.velocity = {0.0f, -ball.speed};
                    } else {
                        ball.velocity = ball.storedVelocity;
                    }
                    ball.storedVelocity = {};
                }
            }
        }

        if (!ball.inPlay) {
            ball.position.x = paddle.rect.x + paddle.rect.width * 0.5f;
            ball.position.y = paddle.rect.y - ball.radius - 1.0f;
        }

        bool canAct = !paused && !gameOver && !gameWon;

        if (canAct && IsKeyPressed(KEY_SPACE)) {
            LaunchBall(ball);
        }

        if (canAct && IsKeyPressed(KEY_Q)) {
            lives = 0;
            gameOver = true;
            ball.inPlay = false;
            if (!gameOverSoundPlayed && gAudioReady) {
                PlaySound(gGameOverSound);
                gameOverSoundPlayed = true;
            }
        }

        if (canAct && ball.inPlay && !ball.frozen) {
            Vector2 previousPosition = ball.position;
            ball.position = {
                ball.position.x + ball.velocity.x * dt,
                ball.position.y + ball.velocity.y * dt,
            };

            HandleBallWallCollisions(ball);
            bool hitPaddle = HandleBallPaddleCollision(ball, paddle);
            if (hitPaddle) {
                if (ball.overloaded) {
                    ShowReactionMessage(reactionMessage, "Overloaded!", BrickPalette[ColorIndexRed]);
                }
                if (ball.superconduct) {
                    ShowReactionMessage(reactionMessage, "Superconduct!", BrickPalette[ColorIndexLightBlue]);
                }
                 if (ball.frozen) {
                     ShowReactionMessage(reactionMessage, "Freeze!", BrickPalette[ColorIndexLightBlue]);
                 }
            }
            score += HandleBallBrickCollision(ball, bricks, previousPosition, reactionEvents, reactionMessage);

            int activeBricks = CountActiveBricks(bricks);

            if (activeBricks == 0) {
                bricks = CreateBricks();
                ball.speed *= 1.15f;
                ball.inPlay = false;
                ResetBallOnPaddle(ball, paddle);
                reactionEvents.clear();
                reactionMessage.active = false;
                gameOverSoundPlayed = false;
            }

            if (ball.position.y - ball.radius > ScreenHeight) {
                lives -= 1;
                if (lives <= 0) {
                    gameOver = true;
                    if (!gameOverSoundPlayed && gAudioReady) {
                        PlaySound(gGameOverSound);
                        gameOverSoundPlayed = true;
                    }
                }
                ResetBallOnPaddle(ball, paddle);
            }
        }

        if (!paused && !gameOver && !gameWon) {
            int extraRemoved = ResolveReactionEvents(dt, reactionEvents, bricks);
            if (extraRemoved > 0) {
                score += extraRemoved;
            }
            if (CountActiveBricks(bricks) == 0) {
                bricks = CreateBricks();
                ball.speed *= 1.15f;
                ball.inPlay = false;
                ResetBallOnPaddle(ball, paddle);
                reactionEvents.clear();
                reactionMessage.active = false;
                gameOverSoundPlayed = false;
            }
        }

        if ((gameOver || gameWon) && IsKeyPressed(KEY_ENTER)) {
            bricks = CreateBricks();
            score = 0;
            lives = 1;
            gameOver = false;
            gameWon = false;
            paused = false;
            ball.speed = 420.0f;
            ResetBallOnPaddle(ball, paddle);
            paddle.rect.x = ScreenWidth / 2.0f - paddle.rect.width * 0.5f;
            paddle.rect.y = ScreenHeight - 80.0f;
            gameOverSoundPlayed = false;
        }

    BeginDrawing();
        ClearBackground(BLACK);

        DrawText("Elemental Breakout", ScreenWidth / 2 - MeasureText("Elemental Breakout", 32) / 2, 24, 32, WHITE);

        for (const Brick& brick : bricks) {
            if (brick.active) {
                Color drawColor = brick.cracked ? brick.color : brick.baseColor;
                DrawRectangleRec(brick.rect, drawColor);
                if (brick.cracked) {
                    DrawRectangleLinesEx(brick.rect, 2.0f, Fade(WHITE, 0.6f));
                } else if (brick.frozen) {
                    DrawRectangleLinesEx(brick.rect, 2.0f, Fade(BLUE, 0.5f));
                }
            }
        }

        DrawRectangleRounded(paddle.rect, 0.9f, 16, paddle.color);
        DrawCircleV(ball.position, ball.radius, ball.color);

        DrawText(TextFormat("Score: %d", score), 40, ScreenHeight - 60, 24, RAYWHITE);
        DrawText(TextFormat("Lives: %d", lives), ScreenWidth - 160, ScreenHeight - 60, 24, RAYWHITE);
        const char* controlsText = "Left/Right or A/D to move, P to pause, Q to quit, 1-5 to change paddle color";
        int controlsWidth = MeasureText(controlsText, 20);
        DrawText(controlsText, ScreenWidth / 2 - controlsWidth / 2, ScreenHeight - 32, 20, GRAY);

        if (reactionMessage.active) {
            int fontSize = 32;
            int textWidth = MeasureText(reactionMessage.text.c_str(), fontSize);
            DrawText(reactionMessage.text.c_str(), ScreenWidth / 2 - textWidth / 2, ScreenHeight - 200, fontSize, reactionMessage.color);
        }
        if (paused && !gameOver && !gameWon) {
            DrawText("Paused - Press P to resume", ScreenWidth / 2 - 170, ScreenHeight / 2, 24, SKYBLUE);
        }
        if (gameOver) {
            DrawText("Game Over - Press ENTER to restart", ScreenWidth / 2 - 220, ScreenHeight / 2, 24, RED);
        } else if (gameWon) {
            DrawText("You Win! Press ENTER to play again", ScreenWidth / 2 - 220, ScreenHeight / 2, 24, GREEN);
        }

        EndDrawing();
    }

    if (gAudioReady) {
        UnloadSound(gBounceSound);
        UnloadSound(gGameOverSound);
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}

