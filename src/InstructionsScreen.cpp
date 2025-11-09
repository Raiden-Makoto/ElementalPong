#include "InstructionsScreen.h"

#include <raylib.h>

#include <cmath>
#include <sstream>

namespace {
const char* kHelpLines[] = {
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
    "Press Enter or Space to begin!"
};
constexpr int kHelpLineCount = sizeof(kHelpLines) / sizeof(kHelpLines[0]);

std::vector<std::string> WrapLines(float maxWidth, int fontSize) {
    std::vector<std::string> wrapped;
    wrapped.reserve(kHelpLineCount * 2);

    for (int i = 0; i < kHelpLineCount; ++i) {
        const std::string line = kHelpLines[i];
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
}  // namespace

void InstructionsScreen::Initialize(int screenWidth, int screenHeight) {
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    scroll_ = 0.0f;
    active_ = true;

    const float panelWidth = static_cast<float>(screenWidth_) - 120.0f;
    const float wrapWidth = panelWidth - 80.0f;
    wrappedLines_ = WrapLines(wrapWidth, 22);
}

void InstructionsScreen::Show() {
    active_ = true;
    scroll_ = 0.0f;
}

void InstructionsScreen::Update(float dt) {
    if (!active_) {
        return;
    }

    scroll_ += GetMouseWheelMove() * -48.0f;
    if (IsKeyDown(KEY_DOWN)) {
        scroll_ += 180.0f * dt;
    }
    if (IsKeyDown(KEY_UP)) {
        scroll_ -= 180.0f * dt;
    }

    Rectangle panelRect{60.0f, 80.0f, static_cast<float>(screenWidth_ - 120), static_cast<float>(screenHeight_ - 160)};
    const int lineSpacing = 28;
    int availableHeight = static_cast<int>(panelRect.height) - 80;
    int totalHeight = static_cast<int>(wrappedLines_.size()) * lineSpacing;

    if (totalHeight < availableHeight) {
        scroll_ = 0.0f;
    } else {
        float maxScroll = static_cast<float>(totalHeight - availableHeight);
        if (scroll_ < 0.0f) scroll_ = 0.0f;
        if (scroll_ > maxScroll) scroll_ = maxScroll;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        active_ = false;
        scroll_ = 0.0f;
    }
}

void InstructionsScreen::Draw() const {
    if (!active_) {
        return;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Elemental Breakout", screenWidth_ / 2 - MeasureText("Elemental Breakout", 48) / 2, 40, 48, WHITE);

    Rectangle panelRect{60.0f, 80.0f, static_cast<float>(screenWidth_ - 120), static_cast<float>(screenHeight_ - 160)};
    DrawRectangleRounded(panelRect, 0.1f, 8, Fade(BLACK, 0.85f));
    DrawRectangleRoundedLines(panelRect, 0.1f, 8, Fade(WHITE, 0.4f));

    const int fontSize = 22;
    const int lineSpacing = 28;
    int baseY = static_cast<int>(panelRect.y) + 40;

    for (size_t i = 0; i < wrappedLines_.size(); ++i) {
        int drawY = baseY + static_cast<int>(i) * lineSpacing - static_cast<int>(scroll_);
        if (drawY < panelRect.y + 30 || drawY > panelRect.y + panelRect.height - 50) {
            continue;
        }
        Color lineColor = (i == 0) ? YELLOW : LIGHTGRAY;
        DrawText(wrappedLines_[i].c_str(), static_cast<int>(panelRect.x) + 40, drawY, fontSize, lineColor);
    }

    int hintY = static_cast<int>(panelRect.y + panelRect.height) + 20;
    const char* hintScroll = "Mouse wheel / Arrow keys to scroll";
    const char* hintStart = "Press Enter or Space to start";
    DrawText(hintScroll, screenWidth_ / 2 - MeasureText(hintScroll, 20) / 2, hintY, 20, GRAY);
    DrawText(hintStart, screenWidth_ / 2 - MeasureText(hintStart, 20) / 2, hintY + 28, 20, GRAY);
    EndDrawing();
}

