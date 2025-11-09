// Basic 960x720 Breakout clone using raylib and C++.
#include <ctime>

#include <raylib.h>

#include "AudioManager.h"
#include "ElementalGame.h"
#include "GameConstants.h"
#include "InstructionsScreen.h"

int main() {
    SetRandomSeed(static_cast<unsigned int>(std::time(nullptr)));
    InitWindow(ScreenWidth, ScreenHeight, "Elemental Breakout");
    SetTargetFPS(60);

    AudioManager audio;
    audio.Init();

    InstructionsScreen instructions;
    instructions.Initialize(ScreenWidth, ScreenHeight);

    ElementalGame game;
    game.Initialize(&audio);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (instructions.IsActive()) {
            instructions.Update(dt);
            instructions.Draw();
            if (!instructions.IsActive()) {
                game.ResetRun();
            }
            continue;
        }

        game.Update(dt);
        game.Draw();
    }

    audio.Shutdown();
    CloseWindow();
    return 0;
}