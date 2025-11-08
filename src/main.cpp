// Basic 960x720 Breakout clone using raylib and C++.
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

struct Paddle {
    Rectangle rect{};
    float speed{640.0f}; // pixels per second
};

struct Ball {
    Vector2 position{};
    Vector2 velocity{};
    float radius{10.0f};
    float speed{420.0f};
    bool inPlay{false};
};

struct Brick {
    Rectangle rect{};
    bool active{true};
    Color color{WHITE};
};

constexpr int ScreenWidth = 960;
constexpr int ScreenHeight = 720;
constexpr int BrickCols = 12;
constexpr int BrickRows = 6;
constexpr float BrickSpacing = 8.0f;
constexpr float BrickHeight = 28.0f;

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
    float topOffset = 100.0f;

    for (int row = 0; row < BrickRows; ++row) {
        for (int col = 0; col < BrickCols; ++col) {
            float x = BrickSpacing + col * (brickWidth + BrickSpacing);
            float y = topOffset + row * (BrickHeight + BrickSpacing);

            Color color = WHITE;
            switch (row % 4) {
                case 0: color = RED; break;
                case 1: color = ORANGE; break;
                case 2: color = YELLOW; break;
                case 3: color = GREEN; break;
            }

            bool hasGap = GetRandomValue(0, 99) < 15; // 15% chance to skip a brick

            bricks.push_back(Brick{
                .rect = {x, y, brickWidth, BrickHeight},
                .active = !hasGap,
                .color = color,
            });
        }
    }

    return bricks;
}

void ResetBallOnPaddle(Ball& ball, const Paddle& paddle) {
    ball.inPlay = false;
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
    return true;
}

int HandleBallBrickCollision(Ball& ball, std::vector<Brick>& bricks, Vector2 previousPosition) {
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

        brick.active = false;
        bricksBroken += 1;

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

        break;
    }
    return bricksBroken;
}

int main() {
    SetRandomSeed(static_cast<unsigned int>(std::time(nullptr)));
    InitWindow(ScreenWidth, ScreenHeight, "Elemental Breakout");
    SetTargetFPS(60);

    Paddle paddle{{ScreenWidth / 2.0f - 60.0f, ScreenHeight - 80.0f, 120.0f, 20.0f}};
    Ball ball{};
    ResetBallOnPaddle(ball, paddle);

    std::vector<Brick> bricks = CreateBricks();

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

        if (!paused && ball.inPlay && !gameOver && !gameWon) {
            Vector2 previousPosition = ball.position;
            ball.position = {
                ball.position.x + ball.velocity.x * dt,
                ball.position.y + ball.velocity.y * dt,
            };

            HandleBallWallCollisions(ball);
            HandleBallPaddleCollision(ball, paddle);
            score += HandleBallBrickCollision(ball, bricks, previousPosition);

            int activeBricks = 0;
            for (const Brick& brick : bricks) {
                if (brick.active) {
                    activeBricks += 1;
                }
            }

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
                DrawRectangleRec(brick.rect, brick.color);
            }
        }

        DrawRectangleRounded(paddle.rect, 0.9f, 16, LIGHTGRAY);
        DrawCircleV(ball.position, ball.radius, WHITE);

        DrawText(TextFormat("Score: %d", score), 40, ScreenHeight - 60, 24, RAYWHITE);
        DrawText(TextFormat("Lives: %d", lives), ScreenWidth - 160, ScreenHeight - 60, 24, RAYWHITE);
        const char* controlsText = "Left/Right or A/D to move, P to pause, Q to quit";
        int controlsWidth = MeasureText(controlsText, 20);
        DrawText(controlsText, ScreenWidth / 2 - controlsWidth / 2, ScreenHeight - 32, 20, GRAY);
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

