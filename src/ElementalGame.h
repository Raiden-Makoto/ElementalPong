#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "GameConstants.h"

class AudioManager;

struct Paddle {
    Rectangle rect{};
    float speed{640.0f};
    int colorIndex{0};
    Color color{WHITE};
};

struct Ball {
    Vector2 position{};
    Vector2 velocity{};
    float radius{12.0f};
    float speed{420.0f};
    bool inPlay{false};
    Color color{WHITE};
    int colorIndex{-1};
    bool overloaded{false};
    bool superconduct{false};
    float superconductTimer{0.0f};
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
    int originalColorIndex{-1};
    Color originalColor{WHITE};
};

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

class ElementalGame {
public:
    ElementalGame();

    void Initialize(AudioManager* audioManager);
    void ResetRun();

    void Update(float dt);
    void Draw() const;

private:
    void LaunchBall();
    void SpawnWave();
    void HandleBallWallCollisions();
    bool HandleBallPaddleCollision();
    int HandleBallBrickCollision();
    int ResolveReactionEvents(float dt);
    void UpdateFreezeState(float dt);
    void ResetBallOnPaddle();
    void ResetPaddlePosition();
    void PlayBounce();
    void PlayGameOver();
    void HandleMovement(float dt);
    void HandlePaddleColorInput();
    void ClearBallStatusEffects();

private:
    Paddle paddle_{};
    Ball ball_{};
    std::vector<Brick> bricks_;
    std::vector<ReactionEvent> reactionEvents_;
    ReactionMessage reactionMessage_{};

    AudioManager* audio_{nullptr};

    int score_{0};
    int lives_{1};
    bool paused_{false};
    bool gameOver_{false};
    bool gameOverSoundPlayed_{false};
    float colorSwitchCooldown_{0.0f};
};

