// Basic 960x720 Breakout clone using raylib and C++.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <queue>
#include <string>
#include <vector>

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
constexpr int BrickRows = 6;
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

struct OverloadEvent {
    int row;
    int col;
    float timer;
};

struct ReactionMessage {
    std::string text{};
    Color color{WHITE};
    float timer{0.0f};
    bool active{false};
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

int ApplyOverloadedAoE(std::vector<Brick>& bricks, int centerRow, int centerCol) {
    int removed = 0;
    for (Brick& brick : bricks) {
        if (!brick.active) {
            continue;
        }
        int dRow = std::abs(brick.row - centerRow);
        int dCol = std::abs(brick.col - centerCol);
        if (dRow <= 1 && dCol <= 1) {
            brick.active = false;
            brick.hitPoints = 0;
            brick.cracked = false;
            brick.color = brick.baseColor;
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

int ResolveOverloadEvents(float dt, std::vector<OverloadEvent>& events, std::vector<Brick>& bricks) {
    int removed = 0;
    for (OverloadEvent& event : events) {
        event.timer -= dt;
    }

    auto it = events.begin();
    while (it != events.end()) {
        if (it->timer <= 0.0f) {
            removed += ApplyOverloadedAoE(bricks, it->row, it->col);
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
    if (ball.position.x - ball.radius <= 0.0f) {
        ball.position.x = ball.radius;
        ball.velocity.x *= -1.0f;
    } else if (ball.position.x + ball.radius >= ScreenWidth) {
        ball.position.x = ScreenWidth - ball.radius;
        ball.velocity.x *= -1.0f;
    }

    if (ball.position.y - ball.radius <= 0.0f) {
        ball.position.y = ball.radius;
        ball.velocity.y *= -1.0f;
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
    return true;
}

int HandleBallBrickCollision(Ball& ball, std::vector<Brick>& bricks, Vector2 previousPosition, std::vector<OverloadEvent>& overloadEvents, ReactionMessage& reactionMessage) {
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
            }

            if (!resolved && (collidedFromTop || collidedFromBottom)) {
                ball.velocity.y *= -1.0f;
                if (collidedFromTop) {
                    ball.position.y = brick.rect.y - ball.radius;
                } else {
                    ball.position.y = brick.rect.y + brick.rect.height + ball.radius;
                }
                resolved = true;
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
                } else {
                    ball.velocity.y *= -1.0f;
                    if (diffY > 0.0f) {
                        ball.position.y = brick.rect.y + brick.rect.height + ball.radius;
                    } else {
                        ball.position.y = brick.rect.y - ball.radius;
                    }
                }
            }
        }

        bool triggeredSwirl = (ball.colorIndex == ColorIndexGreen) &&
                              (brick.colorIndex != ColorIndexGreen) &&
                              (brick.colorIndex != -1);

        bool overloadTriggered = ball.overloaded;
        bool instantBreak = triggeredSwirl || overloadTriggered;
        bool destroyedThisHit = false;

        if (brick.frozen) {
            if (ball.colorIndex == ColorIndexRed) {
                brick.frozen = false;
                brick.baseColor = {255, 221, 0, 255};
                brick.color = brick.baseColor;
                brick.colorIndex = -1;
                brick.hitPoints = 1;
                brick.cracked = false;
            } else {
                instantBreak = true;
            }
        }

        if (instantBreak) {
            brick.active = false;
            brick.hitPoints = 0;
            brick.cracked = false;
            brick.color = brick.baseColor;
            destroyedThisHit = true;
            brick.frozen = false;
        } else {
            brick.hitPoints -= 1;
            if (brick.hitPoints <= 0) {
                brick.active = false;
                brick.hitPoints = 0;
                destroyedThisHit = true;
                brick.frozen = false;
            } else {
                brick.cracked = true;
                brick.color = LightenColor(brick.baseColor, 0.45f);
            }
        }

        if (triggeredSwirl) {
            overloadEvents.push_back(OverloadEvent{
                .row = brick.row,
                .col = brick.col,
                .timer = OverloadAoEDelay,
            });
            ShowReactionMessage(reactionMessage, "Swirl!", BrickPalette[ColorIndexGreen]);
        }

        if (overloadTriggered) {
            overloadEvents.push_back(OverloadEvent{
                .row = brick.row,
                .col = brick.col,
                .timer = OverloadAoEDelay,
            });
            ShowReactionMessage(reactionMessage, "Overloaded!", BrickPalette[ColorIndexRed]);
            ball.overloaded = false;
        }

        if (destroyedThisHit) {
            bricksBroken += 1;
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
    SetTargetFPS(60);

    Paddle paddle{{ScreenWidth / 2.0f - 60.0f, ScreenHeight - 80.0f, 120.0f, 20.0f}};
    paddle.colorIndex = ColorIndexPurple;
    paddle.color = BrickPalette[paddle.colorIndex];

    Ball ball{};
    ball.colorIndex = -1;
    ball.color = WHITE;
    ResetBallOnPaddle(ball, paddle);

    std::vector<Brick> bricks = CreateBricks();
    std::vector<OverloadEvent> overloadEvents;
    ReactionMessage reactionMessage;

    int score = 0;
    int lives = 3;
    bool gameWon = false;
    bool gameOver = false;
    bool paused = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

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

        if (!paused && !gameOver && !gameWon && IsKeyPressed(KEY_SPACE)) {
            LaunchBall(ball);
        }

        if (!paused && !gameOver && !gameWon && IsKeyPressed(KEY_Q)) {
            lives = 0;
            gameOver = true;
            ball.inPlay = false;
        }

        if (!paused && ball.inPlay && !gameOver && !gameWon && !ball.frozen) {
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
            score += HandleBallBrickCollision(ball, bricks, previousPosition, overloadEvents, reactionMessage);

            int activeBricks = CountActiveBricks(bricks);

            if (activeBricks == 0) {
                gameWon = true;
                ball.inPlay = false;
            }

            if (ball.position.y - ball.radius > ScreenHeight) {
                lives -= 1;
                if (lives <= 0) {
                    gameOver = true;
                }
                ResetBallOnPaddle(ball, paddle);
            }
        }

        if (!paused && !gameOver && !gameWon) {
            int extraRemoved = ResolveOverloadEvents(dt, overloadEvents, bricks);
            if (extraRemoved > 0) {
                score += extraRemoved;
                if (CountActiveBricks(bricks) == 0) {
                    gameWon = true;
                    ball.inPlay = false;
                }
            }
        }

        if ((gameOver || gameWon) && IsKeyPressed(KEY_ENTER)) {
            bricks = CreateBricks();
            score = 0;
            lives = 3;
            gameOver = false;
            gameWon = false;
            paused = false;
            ResetBallOnPaddle(ball, paddle);
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

    CloseWindow();
    return 0;
}

