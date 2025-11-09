#pragma once

#include <raylib.h>

#include <string>
#include <vector>

class InstructionsScreen {
public:
    InstructionsScreen() = default;

    void Initialize(int screenWidth, int screenHeight);
    void Show();
    bool IsActive() const { return active_; }

    void Update(float dt);
    void Draw() const;

private:
    std::vector<std::string> wrappedLines_;
    int screenWidth_{0};
    int screenHeight_{0};
    float scroll_{0.0f};
    bool active_{true};
};

