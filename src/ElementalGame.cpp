#include "ElementalGame.h"

#include "AudioManager.h"
#include "GameConstants.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace {
const Color kBrickPalette[] = {
    {255, 102, 0, 255},   // orange-red
    {0, 112, 221, 255},   // blue
    {0, 191, 165, 255},   // teal-green
    {196, 120, 255, 255}, // light purple
    {173, 216, 230, 255}, // light blue/white
};
constexpr int kBrickPaletteCount = sizeof(kBrickPalette) / sizeof(kBrickPalette[0]);

constexpr int kColorIndexRed = 0;
constexpr int kColorIndexBlue = 1;
constexpr int kColorIndexGreen = 2;
constexpr int kColorIndexPurple = 3;
constexpr int kColorIndexLightBlue = 4;

Brick* GetBrickAt(std::vector<Brick>& bricks, int row, int col) {
    for (Brick& brick : bricks) {
        if (brick.row == row && brick.col == col) {
            return &brick;
        }
    }
    return nullptr;
}

void DestroyBrick(Brick& brick) {
    brick.active = false;
    brick.hitPoints = 0;
    brick.cracked = false;
    brick.frozen = false;
    brick.color = brick.baseColor;
    brick.colorIndex = -1;
}

int FreezeConnectedBricks(std::vector<Brick>& bricks, int startRow, int startCol, int targetColorIndex) {
    bool visited[BrickRows][BrickCols] = {};
    std::queue<std::pair<int, int>> toVisit;
    toVisit.emplace(startRow, startCol);

    const std::pair<int, int> directions[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int frozenCount = 0;

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
        if (brick == nullptr || !brick->active) {
            continue;
        }
        if (brick->colorIndex != targetColorIndex) {
            continue;
        }

        brick->originalColorIndex = brick->colorIndex;
        brick->originalColor = brick->baseColor;
        brick->frozen = true;
        brick->baseColor = WHITE;
        brick->color = WHITE;
        frozenCount += 1;

        for (const auto& dir : directions) {
            toVisit.emplace(row + dir.first, col + dir.second);
        }
    }

    return frozenCount;
}

void ThawFrozenCluster(std::vector<Brick>& bricks, int startRow, int startCol) {
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
        if (brick == nullptr || !brick->active || !brick->frozen) {
            continue;
        }

        brick->frozen = false;
        brick->colorIndex = kColorIndexBlue;
        brick->baseColor = kBrickPalette[kColorIndexBlue];
        brick->color = brick->baseColor;

        for (const auto& dir : directions) {
            toVisit.emplace(row + dir.first, col + dir.second);
        }
    }
}

void ScheduleSurgeChain(std::vector<ReactionEvent>& events, std::vector<Brick>& bricks, int startRow, int startCol) {
    const std::pair<int, int> directions[] = {{1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
    int scheduled = 0;
    for (const auto& dir : directions) {
        int row = startRow + dir.first;
        int col = startCol + dir.second;
        int distance = 1;
        while (row >= 0 && row < BrickRows && col >= 0 && col < BrickCols) {
            Brick* target = GetBrickAt(bricks, row, col);
            if (target != nullptr && target->active) {
                events.push_back(ReactionEvent{row, col, SurgeChainStepDelay * static_cast<float>(distance), ReactionKind::SurgeChain});
                scheduled += 1;
                if (scheduled >= 4) {
                    return;
                }
            }
            row += dir.first;
            col += dir.second;
            distance += 1;
        }
    }
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

            int colorIdx = -1;
            Color chunkColor = {255, 221, 0, 255};  // default to yellow

            int roll = GetRandomValue(1, 100);
            if (roll <= 60) {
                colorIdx = -1;
                chunkColor = {255, 221, 0, 255};
            } else if (roll <= 64) {
                colorIdx = kColorIndexGreen;
                chunkColor = kBrickPalette[colorIdx];
            } else {
                static const int kRemainingColors[] = {
                    kColorIndexRed,
                    kColorIndexBlue,
                    kColorIndexPurple,
                    kColorIndexLightBlue,
                };
                int remainder = roll - 64;  // 1-36
                int index = (remainder - 1) / 9;
                if (index < 0) {
                    index = 0;
                } else if (index > 3) {
                    index = 3;
                }
                colorIdx = kRemainingColors[index];
                chunkColor = kBrickPalette[colorIdx];
            }

            for (int i = 0; i < chunkSize; ++i) {
                int currentCol = col + i;
                float x = BrickSpacing + currentCol * (brickWidth + BrickSpacing);
                float y = BrickTopOffset + row * (BrickHeight + BrickSpacing);

                bool hasGap = GetRandomValue(0, 99) < 17;
                if (hasGap) {
                    continue;
                }

                bricks.push_back(Brick{
                    {x, y, brickWidth, BrickHeight},
                    true,
                    chunkColor,
                    chunkColor,
                    row,
                    currentCol,
                    colorIdx,
                    2,
                    false,
                    false,
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
            DestroyBrick(brick);
            removed += 1;
        }
    }
    return removed;
}

int CountActiveBricks(const std::vector<Brick>& bricks) {
    int count = 0;
    for (const Brick& brick : bricks) {
        if (brick.active) {
            ++count;
        }
    }
    return count;
}
}  // namespace

ElementalGame::ElementalGame() = default;

void ElementalGame::Initialize(AudioManager* audioManager) {
    audio_ = audioManager;
    ResetRun();
}

void ElementalGame::ResetRun() {
    score_ = 0;
    lives_ = 1;
    paused_ = false;
    gameOver_ = false;
    gameOverSoundPlayed_ = false;
    reactionEvents_.clear();
    reactionMessage_ = {};

    paddle_.speed = 640.0f;
    paddle_.rect.width = 120.0f;
    paddle_.rect.height = 20.0f;
    ResetPaddlePosition();
    paddle_.colorIndex = kColorIndexPurple;
    paddle_.color = kBrickPalette[paddle_.colorIndex];

    ball_ = {};
    ball_.radius = 12.0f;
    ball_.speed = 420.0f;
    ball_.color = WHITE;
    ball_.colorIndex = -1;
    ResetBallOnPaddle();

    bricks_ = CreateBricks();
    colorSwitchCooldown_ = 0.0f;
    ball_.superconductTimer = 0.0f;
}

void ElementalGame::ResetBallOnPaddle() {
    ball_.inPlay = false;
    ball_.position = {paddle_.rect.x + paddle_.rect.width * 0.5f, paddle_.rect.y - ball_.radius - 1.0f};
    ball_.velocity = {0.0f, 0.0f};
    ClearBallStatusEffects();
    reactionMessage_.active = false;
    reactionMessage_.text.clear();
    reactionMessage_.timer = 0.0f;
}

void ElementalGame::ClearBallStatusEffects() {
    if (ball_.frozen) {
        float storedSpeed = std::sqrt(ball_.storedVelocity.x * ball_.storedVelocity.x + ball_.storedVelocity.y * ball_.storedVelocity.y);
        if (storedSpeed <= 0.001f) {
            ball_.velocity = {0.0f, -ball_.speed};
        } else {
            ball_.velocity = ball_.storedVelocity;
        }
    }

    ball_.overloaded = false;
    ball_.superconduct = false;
    ball_.superconductTimer = 0.0f;
    ball_.frozen = false;
    ball_.freezeReady = false;
    ball_.freezeTimer = 0.0f;
    ball_.storedVelocity = {};
    ball_.vaporizeReady = false;
}

void ElementalGame::ResetPaddlePosition() {
    paddle_.rect.x = ScreenWidth / 2.0f - paddle_.rect.width * 0.5f;
    paddle_.rect.y = ScreenHeight - 80.0f;
}

void ElementalGame::HandleMovement(float dt) {
    float dx = 0.0f;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        dx -= paddle_.speed * dt;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        dx += paddle_.speed * dt;
    }

    paddle_.rect.x += dx;
    if (paddle_.rect.x < 0.0f) {
        paddle_.rect.x = 0.0f;
    }
    if (paddle_.rect.x + paddle_.rect.width > ScreenWidth) {
        paddle_.rect.x = ScreenWidth - paddle_.rect.width;
    }
}

void ElementalGame::HandlePaddleColorInput() {
    if (colorSwitchCooldown_ > 0.0f) {
        return;
    }

    const int keys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE};
    for (int i = 0; i < kBrickPaletteCount && i < static_cast<int>(sizeof(keys) / sizeof(keys[0])); ++i) {
        if (IsKeyPressed(keys[i])) {
            paddle_.colorIndex = i;
            paddle_.color = kBrickPalette[i];
            colorSwitchCooldown_ = 3.0f;
            break;
        }
    }
}

void ElementalGame::SpawnWave() {
    bricks_ = CreateBricks();
    reactionEvents_.clear();
    reactionMessage_ = {};
    ResetBallOnPaddle();
    gameOverSoundPlayed_ = false;
}

void ElementalGame::LaunchBall() {
    if (ball_.inPlay) {
        return;
    }

    float direction = GetRandomValue(0, 1) == 0 ? -1.0f : 1.0f;
    Vector2 initialDir{direction * 0.6f, -1.0f};
    float lengthSq = initialDir.x * initialDir.x + initialDir.y * initialDir.y;
    if (lengthSq > 0.0f) {
        float invLength = 1.0f / std::sqrt(lengthSq);
        initialDir.x *= invLength;
        initialDir.y *= invLength;
    }

    ball_.velocity = {initialDir.x * ball_.speed, initialDir.y * ball_.speed};
    ball_.inPlay = true;
}

void ElementalGame::PlayBounce() {
    if (audio_) {
        audio_->PlayBounce();
    }
}

void ElementalGame::PlayGameOver() {
    if (audio_ && !gameOverSoundPlayed_) {
        audio_->PlayGameOver();
        gameOverSoundPlayed_ = true;
    }
}

void ElementalGame::HandleBallWallCollisions() {
    bool bounced = false;
    if (ball_.position.x - ball_.radius <= 0.0f) {
        ball_.position.x = ball_.radius;
        ball_.velocity.x *= -1.0f;
        bounced = true;
    } else if (ball_.position.x + ball_.radius >= ScreenWidth) {
        ball_.position.x = ScreenWidth - ball_.radius;
        ball_.velocity.x *= -1.0f;
        bounced = true;
    }

    if (ball_.position.y - ball_.radius <= 0.0f) {
        ball_.position.y = ball_.radius;
        ball_.velocity.y *= -1.0f;
        bounced = true;
    }

    if (bounced) {
        PlayBounce();
    }
}

bool ElementalGame::HandleBallPaddleCollision() {
    if (!ball_.inPlay) {
        return false;
    }

    if (!CheckCollisionCircleRec(ball_.position, ball_.radius, paddle_.rect)) {
        return false;
    }

    ball_.position.y = paddle_.rect.y - ball_.radius - 1.0f;
    float paddleCenter = paddle_.rect.x + paddle_.rect.width * 0.5f;
    float relative = (ball_.position.x - paddleCenter) / (paddle_.rect.width * 0.5f);
    relative = std::clamp(relative, -1.0f, 1.0f);

    Vector2 direction{relative, -1.0f};
    float lengthSq = direction.x * direction.x + direction.y * direction.y;
    if (lengthSq > 0.0f) {
        float invLength = 1.0f / std::sqrt(lengthSq);
        direction.x *= invLength;
        direction.y *= invLength;
    }
    ball_.velocity = {direction.x * ball_.speed, direction.y * ball_.speed};

    bool overloadedTrigger = (ball_.colorIndex == kColorIndexPurple && paddle_.colorIndex == kColorIndexRed) ||
                             (ball_.colorIndex == kColorIndexRed && paddle_.colorIndex == kColorIndexPurple);
    bool superconductTrigger = (ball_.colorIndex == kColorIndexPurple && paddle_.colorIndex == kColorIndexLightBlue) ||
                               (ball_.colorIndex == kColorIndexLightBlue && paddle_.colorIndex == kColorIndexPurple);
    bool freezeTrigger = (ball_.colorIndex == kColorIndexBlue && paddle_.colorIndex == kColorIndexLightBlue) ||
                         (ball_.colorIndex == kColorIndexLightBlue && paddle_.colorIndex == kColorIndexBlue);

    ClearBallStatusEffects();

    if (paddle_.colorIndex >= 0 && paddle_.colorIndex < kBrickPaletteCount) {
        ball_.colorIndex = paddle_.colorIndex;
        ball_.color = kBrickPalette[paddle_.colorIndex];
    } else {
        ball_.colorIndex = -1;
        ball_.color = WHITE;
    }

    ball_.overloaded = overloadedTrigger;
    ball_.superconduct = superconductTrigger;
    ball_.superconductTimer = superconductTrigger ? 1.0f : 0.0f;

    if (freezeTrigger) {
        ball_.freezeReady = true;
        ball_.frozen = true;
        ball_.freezeTimer = 2.0f;
        ball_.storedVelocity = ball_.velocity;
        ball_.velocity = {0.0f, 0.0f};
    } else {
        ball_.freezeReady = false;
        ball_.frozen = false;
        ball_.freezeTimer = 0.0f;
        ball_.storedVelocity = {};
    }

    ball_.vaporizeReady = false;
    PlayBounce();
    return true;
}

int ElementalGame::HandleBallBrickCollision() {
    int bricksBroken = 0;

    for (Brick& brick : bricks_) {
        if (!brick.active) {
            continue;
        }
        if (!CheckCollisionCircleRec(ball_.position, ball_.radius, brick.rect)) {
            continue;
        }

        int freezeColorIndex = brick.colorIndex;

        if (ball_.freezeReady) {
            int target = freezeColorIndex;
            if (target != kColorIndexLightBlue) {
                int frozenBricks = FreezeConnectedBricks(bricks_, brick.row, brick.col, target);
                if (frozenBricks > 0) {
                    reactionMessage_.text = "Freeze!";
                    reactionMessage_.color = kBrickPalette[kColorIndexLightBlue];
                    reactionMessage_.timer = 1.0f;
                    reactionMessage_.active = true;
                }
            }
            ball_.freezeReady = false;
        }

        bool brickBounced = false;

        if (!ball_.superconduct) {
            bool collidedFromLeft = ball_.position.x + ball_.radius <= brick.rect.x;
            bool collidedFromRight = ball_.position.x - ball_.radius >= brick.rect.x + brick.rect.width;
            bool collidedFromTop = ball_.position.y + ball_.radius <= brick.rect.y;
            bool collidedFromBottom = ball_.position.y - ball_.radius >= brick.rect.y + brick.rect.height;

            bool resolved = false;

            if (collidedFromLeft || collidedFromRight) {
                ball_.velocity.x *= -1.0f;
                if (collidedFromLeft) {
                    ball_.position.x = brick.rect.x - ball_.radius;
                } else {
                    ball_.position.x = brick.rect.x + brick.rect.width + ball_.radius;
                }
                resolved = true;
                brickBounced = true;
            }

            if (!resolved && (collidedFromTop || collidedFromBottom)) {
                ball_.velocity.y *= -1.0f;
                if (collidedFromTop) {
                    ball_.position.y = brick.rect.y - ball_.radius;
                } else {
                    ball_.position.y = brick.rect.y + brick.rect.height + ball_.radius;
                }
                resolved = true;
                brickBounced = true;
            }

            if (!resolved) {
                float brickCenterX = brick.rect.x + brick.rect.width * 0.5f;
                float brickCenterY = brick.rect.y + brick.rect.height * 0.5f;
                float diffX = ball_.position.x - brickCenterX;
                float diffY = ball_.position.y - brickCenterY;

                if (std::abs(diffX) > std::abs(diffY)) {
                    ball_.velocity.x *= -1.0f;
                    if (diffX > 0.0f) {
                        ball_.position.x = brick.rect.x + brick.rect.width + ball_.radius;
                    } else {
                        ball_.position.x = brick.rect.x - ball_.radius;
                    }
                    brickBounced = true;
                } else {
                    ball_.velocity.y *= -1.0f;
                    if (diffY > 0.0f) {
                        ball_.position.y = brick.rect.y + brick.rect.height + ball_.radius;
                    } else {
                        ball_.position.y = brick.rect.y - ball_.radius;
                    }
                    brickBounced = true;
                }
            }
        }

        if (brickBounced) {
            PlayBounce();
        }

        if (brick.frozen) {
            if (ball_.colorIndex == kColorIndexRed) {
                ball_.colorIndex = kColorIndexBlue;
                ball_.color = kBrickPalette[kColorIndexBlue];
                ball_.frozen = false;
                ball_.freezeReady = false;
                ball_.freezeTimer = 0.0f;
                ball_.storedVelocity = {};
                ball_.vaporizeReady = false;

                ThawFrozenCluster(bricks_, brick.row, brick.col);
            } else {
                ball_.frozen = false;
                ball_.freezeReady = false;
                ball_.freezeTimer = 0.0f;
                ball_.storedVelocity = {};
            }
            continue;
        }

        bool triggeredSwirl = (ball_.colorIndex == kColorIndexGreen) &&
                              (brick.colorIndex != kColorIndexGreen) &&
                              (brick.colorIndex != -1);

        bool overloadTriggered = ball_.overloaded;
        bool instantBreak = triggeredSwirl || overloadTriggered;
        bool destroyedThisHit = false;
        bool vaporizeTriggered = false;
        bool infuseTriggered = false;
        bool meltTriggered = false;
        bool liquefyTriggered = false;
        bool surgeTriggered = false;

        if ((ball_.colorIndex == kColorIndexBlue && brick.colorIndex == kColorIndexRed) ||
            (ball_.colorIndex == kColorIndexRed && brick.colorIndex == kColorIndexBlue)) {
            instantBreak = true;
            vaporizeTriggered = true;
            reactionMessage_.text = "Vaporize!";
            reactionMessage_.color = kBrickPalette[kColorIndexBlue];
            reactionMessage_.timer = 1.0f;
            reactionMessage_.active = true;
        } else if (ball_.colorIndex == kColorIndexLightBlue && brick.colorIndex == kColorIndexRed) {
            liquefyTriggered = true;
            reactionMessage_.text = "Liquefy!";
            reactionMessage_.color = kBrickPalette[kColorIndexBlue];
            reactionMessage_.timer = 1.0f;
            reactionMessage_.active = true;
        } else if ((ball_.colorIndex == kColorIndexPurple && brick.colorIndex == kColorIndexBlue) ||
                   (ball_.colorIndex == kColorIndexBlue && brick.colorIndex == kColorIndexPurple)) {
            surgeTriggered = true;
            instantBreak = true;
            reactionMessage_.text = "Surge!";
            reactionMessage_.color = kBrickPalette[kColorIndexPurple];
            reactionMessage_.timer = 1.0f;
            reactionMessage_.active = true;
        } else if (ball_.colorIndex != kColorIndexGreen && brick.colorIndex == kColorIndexGreen) {
            int infused = FreezeConnectedBricks(bricks_, brick.row, brick.col, kColorIndexGreen);
            if (infused > 0) {
                infuseTriggered = true;
                reactionMessage_.text = "Infuse!";
                reactionMessage_.color = kBrickPalette[kColorIndexGreen];
                reactionMessage_.timer = 1.0f;
                reactionMessage_.active = true;
            }
        }

        if (instantBreak) {
            DestroyBrick(brick);
            destroyedThisHit = true;
        } else if (liquefyTriggered) {
            brick.baseColor = kBrickPalette[kColorIndexBlue];
            brick.color = brick.baseColor;
            brick.colorIndex = kColorIndexBlue;
            brick.cracked = false;
            brick.hitPoints = std::max(brick.hitPoints, 2);
        } else if (infuseTriggered) {
            brick.baseColor = ball_.color;
            brick.color = ball_.color;
            brick.colorIndex = ball_.colorIndex;
        } else {
            brick.hitPoints -= 1;
            if (brick.hitPoints <= 0) {
                DestroyBrick(brick);
                destroyedThisHit = true;
            } else {
                brick.cracked = true;
                brick.color = Color{
                    static_cast<unsigned char>(std::clamp<int>(static_cast<int>(brick.baseColor.r * 0.65f), 0, 255)),
                    static_cast<unsigned char>(std::clamp<int>(static_cast<int>(brick.baseColor.g * 0.65f), 0, 255)),
                    static_cast<unsigned char>(std::clamp<int>(static_cast<int>(brick.baseColor.b * 0.65f), 0, 255)),
                    brick.baseColor.a,
                };
            }
        }

        if (triggeredSwirl) {
            reactionEvents_.push_back(ReactionEvent{
                brick.row,
                brick.col,
                OverloadAoEDelay,
                ReactionKind::OverloadAoE,
            });
            reactionMessage_.text = "Swirl!";
            reactionMessage_.color = kBrickPalette[kColorIndexGreen];
            reactionMessage_.timer = 1.0f;
            reactionMessage_.active = true;
        }

        if (overloadTriggered) {
            reactionEvents_.push_back(ReactionEvent{
                brick.row,
                brick.col,
                OverloadAoEDelay,
                ReactionKind::OverloadAoE,
            });
            reactionMessage_.text = "Overloaded!";
            reactionMessage_.color = kBrickPalette[kColorIndexRed];
            reactionMessage_.timer = 1.0f;
            reactionMessage_.active = true;
            ball_.overloaded = false;
        }

        if (destroyedThisHit) {
            bricksBroken += 1;
            if (surgeTriggered) {
                ScheduleSurgeChain(reactionEvents_, bricks_, brick.row, brick.col);
            }
        }

        break;
    }

    return bricksBroken;
}

int ElementalGame::ResolveReactionEvents(float dt) {
    int removed = 0;
    for (ReactionEvent& event : reactionEvents_) {
        event.timer -= dt;
    }

    auto it = reactionEvents_.begin();
    while (it != reactionEvents_.end()) {
        if (it->timer <= 0.0f) {
            if (it->kind == ReactionKind::OverloadAoE) {
                removed += ApplyOverloadedAoE(bricks_, it->row, it->col);
            } else if (it->kind == ReactionKind::SurgeChain) {
                Brick* target = GetBrickAt(bricks_, it->row, it->col);
                if (target && target->active) {
                    DestroyBrick(*target);
                    removed += 1;
                }
            }
            it = reactionEvents_.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

void ElementalGame::UpdateFreezeState(float dt) {
    if (!ball_.inPlay || !ball_.frozen) {
        return;
    }

    ball_.freezeTimer -= dt;
    ball_.position.x = paddle_.rect.x + paddle_.rect.width * 0.5f;
    ball_.position.y = paddle_.rect.y - ball_.radius - 1.0f;
    if (ball_.freezeTimer <= 0.0f) {
        ball_.frozen = false;
        float storedSpeed = std::sqrt(ball_.storedVelocity.x * ball_.storedVelocity.x + ball_.storedVelocity.y * ball_.storedVelocity.y);
        if (storedSpeed <= 0.001f) {
            ball_.velocity = {0.0f, -ball_.speed};
        } else {
            ball_.velocity = ball_.storedVelocity;
        }
        ball_.storedVelocity = {};
    }
}

void ElementalGame::Update(float dt) {
    if (!gameOver_ && IsKeyPressed(KEY_P)) {
        paused_ = !paused_;
    }

    if (!paused_) {
        UpdateFreezeState(dt);
        if (ball_.superconduct && ball_.superconductTimer > 0.0f) {
            ball_.superconductTimer -= dt;
            if (ball_.superconductTimer <= 0.0f) {
                ball_.superconduct = false;
                ball_.superconductTimer = 0.0f;
            }
        }
        if (ball_.superconduct && ball_.superconductTimer > 0.0f) {
            ball_.superconductTimer -= dt;
            if (ball_.superconductTimer <= 0.0f) {
                ball_.superconduct = false;
                ball_.superconductTimer = 0.0f;
            }
        }
        if (colorSwitchCooldown_ > 0.0f) {
            colorSwitchCooldown_ = std::max(0.0f, colorSwitchCooldown_ - dt);
        }
        if (reactionMessage_.active) {
            reactionMessage_.timer -= dt;
            if (reactionMessage_.timer <= 0.0f) {
                reactionMessage_.active = false;
                reactionMessage_.text.clear();
            }
        }
    }

    if (!paused_ && !gameOver_) {
        HandleMovement(dt);
        HandlePaddleColorInput();
    }

    if (!ball_.inPlay) {
        ball_.position.x = paddle_.rect.x + paddle_.rect.width * 0.5f;
        ball_.position.y = paddle_.rect.y - ball_.radius - 1.0f;
    }

    bool canAct = !paused_ && !gameOver_;

    if (canAct && IsKeyPressed(KEY_SPACE)) {
        LaunchBall();
    }

    if (canAct && IsKeyPressed(KEY_Q)) {
        lives_ = 0;
        gameOver_ = true;
        ball_.inPlay = false;
        PlayGameOver();
    }

    if (canAct && ball_.inPlay && !ball_.frozen) {
        ball_.position.x += ball_.velocity.x * dt;
        ball_.position.y += ball_.velocity.y * dt;

        HandleBallWallCollisions();
        bool hitPaddle = HandleBallPaddleCollision();
        if (hitPaddle) {
            if (ball_.overloaded) {
                reactionMessage_.text = "Overloaded!";
                reactionMessage_.color = kBrickPalette[kColorIndexRed];
                reactionMessage_.timer = 1.0f;
                reactionMessage_.active = true;
            }
            if (ball_.superconduct) {
                reactionMessage_.text = "Superconduct!";
                reactionMessage_.color = kBrickPalette[kColorIndexLightBlue];
                reactionMessage_.timer = 1.0f;
                reactionMessage_.active = true;
            }
            if (ball_.frozen) {
                reactionMessage_.text = "Freeze!";
                reactionMessage_.color = kBrickPalette[kColorIndexLightBlue];
                reactionMessage_.timer = 1.0f;
                reactionMessage_.active = true;
            }
        }
        score_ += HandleBallBrickCollision();

        if (CountActiveBricks(bricks_) == 0) {
            SpawnWave();
            ball_.speed *= 1.15f;
        }

        if (ball_.position.y - ball_.radius > ScreenHeight) {
            lives_ -= 1;
            if (lives_ <= 0) {
                gameOver_ = true;
                PlayGameOver();
            }
            ResetBallOnPaddle();
        }
    }

    if (!paused_ && !gameOver_) {
        int extraRemoved = ResolveReactionEvents(dt);
        if (extraRemoved > 0) {
            score_ += extraRemoved;
        }
        if (CountActiveBricks(bricks_) == 0) {
            SpawnWave();
            ball_.speed *= 1.15f;
        }
    }

    if (gameOver_ && IsKeyPressed(KEY_ENTER)) {
        ResetRun();
    }
}

void ElementalGame::Draw() const {
    BeginDrawing();
    ClearBackground(BLACK);

    DrawText("Elemental Breakout", ScreenWidth / 2 - MeasureText("Elemental Breakout", 32) / 2, 24, 32, WHITE);

    for (const Brick& brick : bricks_) {
        if (!brick.active) {
            continue;
        }
        Color drawColor = brick.cracked ? brick.color : brick.baseColor;
        DrawRectangleRec(brick.rect, drawColor);
        if (brick.cracked) {
            DrawRectangleLinesEx(brick.rect, 2.0f, Fade(WHITE, 0.6f));
        } else if (brick.frozen) {
            DrawRectangleLinesEx(brick.rect, 2.0f, Fade(BLUE, 0.5f));
        }
    }

    DrawRectangleRounded(paddle_.rect, 0.9f, 16, paddle_.color);
    DrawCircleV(ball_.position, ball_.radius, ball_.color);

    DrawText(TextFormat("Score: %d", score_), 40, ScreenHeight - 60, 24, RAYWHITE);
    DrawText(TextFormat("Lives: %d", lives_), ScreenWidth - 160, ScreenHeight - 60, 24, RAYWHITE);

    const char* controlsText = "Left/Right or A/D to move, P to pause, Q to quit, 1-5 to change paddle color";
    int controlsWidth = MeasureText(controlsText, 20);
    DrawText(controlsText, ScreenWidth / 2 - controlsWidth / 2, ScreenHeight - 32, 20, GRAY);

    if (reactionMessage_.active) {
        int fontSize = 32;
        int textWidth = MeasureText(reactionMessage_.text.c_str(), fontSize);
        DrawText(reactionMessage_.text.c_str(), ScreenWidth / 2 - textWidth / 2, ScreenHeight - 200, fontSize, reactionMessage_.color);
    }
    if (paused_ && !gameOver_) {
        DrawText("Paused - Press P to resume", ScreenWidth / 2 - 170, ScreenHeight / 2, 24, SKYBLUE);
    }
    if (gameOver_) {
        DrawText("Game Over - Press ENTER to restart", ScreenWidth / 2 - 220, ScreenHeight / 2, 24, RED);
    }

    EndDrawing();
}

