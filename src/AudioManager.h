#pragma once

#include <raylib.h>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    void Init();
    void Shutdown();

    void PlayBounce();
    void PlayGameOver();

    bool IsReady() const { return ready_; }

private:
    bool ready_{false};
    Sound bounceSound_{};
    Sound gameOverSound_{};
};

