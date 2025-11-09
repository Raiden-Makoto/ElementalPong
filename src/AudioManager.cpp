#include "AudioManager.h"

#include <raylib.h>

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    Shutdown();
}

void AudioManager::Init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        return;
    }

    bounceSound_ = LoadSound("sounds/bounce.mp3");
    gameOverSound_ = LoadSound("sounds/gameover.mp3");
}

void AudioManager::Shutdown() {
    if (!ready_) {
        return;
    }

    UnloadSound(bounceSound_);
    UnloadSound(gameOverSound_);
    CloseAudioDevice();
    ready_ = false;
}

void AudioManager::PlayBounce() {
    if (ready_) {
        PlaySound(bounceSound_);
    }
}

void AudioManager::PlayGameOver() {
    if (ready_) {
        PlaySound(gameOverSound_);
    }
}

