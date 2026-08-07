#include "PianoWidget.h"
#include <algorithm>
#include <cstring>

namespace {
    struct KeyDef {
        int midiNote;
        char label;
        bool isBlack;
        int whiteIndex; // 0..13
    };

    // Bottom Keyboard Row (Left: G2..F3 [Y X C V B N M], Right: G3..F4 [y x c v b n m])
    static const KeyDef kBottomWhiteKeys[14] = {
        {43, 'Y', false, 0},  // G2 (Lowest tone, first line of Bass Clef)
        {45, 'X', false, 1},  // A2
        {47, 'C', false, 2},  // B2
        {48, 'V', false, 3},  // C3
        {50, 'B', false, 4},  // D3
        {52, 'N', false, 5},  // E3
        {53, 'M', false, 6},  // F3

        {55, 'y', false, 7},  // G3
        {57, 'x', false, 8},  // A3
        {59, 'c', false, 9},  // B3
        {60, 'v', false, 10}, // C4 (Middle C)
        {62, 'b', false, 11}, // D4
        {64, 'n', false, 12}, // E4
        {65, 'm', false, 13}  // F4
    };

    static const KeyDef kBottomBlackKeys[10] = {
        {44, 'S', true, 0},  // G#2
        {46, 'D', true, 1},  // A#2
        {49, 'G', true, 3},  // C#3
        {51, 'H', true, 4},  // D#3
        {54, 'J', true, 5},  // F#3

        {56, 's', true, 7},  // G#3
        {58, 'd', true, 8},  // A#3
        {61, 'g', true, 10}, // C#4
        {63, 'h', true, 11}, // D#4
        {66, 'j', true, 12}  // F#4
    };

    // Top Keyboard Row (Left: G4..F5 [q w e r t z u], Right: G5..F6 [Q W E R T Z U])
    static const KeyDef kTopWhiteKeys[14] = {
        {67, 'q', false, 0},  // G4
        {69, 'w', false, 1},  // A4
        {71, 'e', false, 2},  // B4
        {72, 'r', false, 3},  // C5
        {74, 't', false, 4},  // D5
        {76, 'z', false, 5},  // E5
        {77, 'u', false, 6},  // F5

        {79, 'Q', false, 7},  // G5
        {81, 'W', false, 8},  // A5
        {83, 'E', false, 9},  // B5
        {84, 'R', false, 10}, // C6
        {86, 'T', false, 11}, // D6
        {88, 'Z', false, 12}, // E6
        {89, 'U', false, 13}  // F6 (Highest tone)
    };

    static const KeyDef kTopBlackKeys[10] = {
        {68, '2', true, 0},  // G#4
        {70, '3', true, 1},  // A#4
        {73, '5', true, 3},  // C#5
        {75, '6', true, 4},  // D#5
        {78, '7', true, 5},  // F#5

        {80, '!', true, 7},  // G#5
        {82, '"', true, 8},  // A#5
        {85, '$', true, 10}, // C#6
        {87, '&', true, 11}, // D#6
        {90, '/', true, 12}  // F#6
    };

    void renderRow(SDL_Renderer* renderer, int startX, int rowY, int whiteW, int rowHeight,
                   const KeyDef whiteKeys[], int numWhite,
                   const KeyDef blackKeys[], int numBlack,
                   const PianoWidget& widget, PianoWidget::RenderTextFn renderText) {

        // 1. White Keys
        for (int i = 0; i < numWhite; ++i) {
            const auto& keyDef = whiteKeys[i];
            int keyX = startX + i * whiteW;
            SDL_Rect keyRect = { keyX, rowY, whiteW - 1, rowHeight };

            if (widget.isKeyPressed(keyDef.midiNote)) {
                SDL_SetRenderDrawColor(renderer, 0, 210, 240, 255); // Highlight cyan
            } else {
                SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
            }
            SDL_RenderFillRect(renderer, &keyRect);

            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
            SDL_RenderDrawRect(renderer, &keyRect);

            // Key Label
            char strBuf[2] = { keyDef.label, '\0' };
            int labelScale = 1;
            int textX = keyX + (whiteW - 6 * labelScale) / 2;
            int textY = rowY + rowHeight - 15;

            if (widget.isKeyPressed(keyDef.midiNote)) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            }
            renderText(strBuf, textX, textY, labelScale);
        }

        // 2. Black Keys
        int blackW = static_cast<int>(whiteW * 0.62f);
        int blackH = static_cast<int>(rowHeight * 0.60f);

        for (int i = 0; i < numBlack; ++i) {
            const auto& keyDef = blackKeys[i];
            int leftWhiteX = startX + keyDef.whiteIndex * whiteW;
            int blackX = leftWhiteX + whiteW - blackW / 2;

            SDL_Rect blackRect = { blackX, rowY, blackW, blackH };

            if (widget.isKeyPressed(keyDef.midiNote)) {
                SDL_SetRenderDrawColor(renderer, 255, 180, 0, 255); // Gold highlight
            } else {
                SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
            }
            SDL_RenderFillRect(renderer, &blackRect);

            SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
            SDL_RenderDrawRect(renderer, &blackRect);

            // Key Label
            char strBuf[2] = { keyDef.label, '\0' };
            int labelScale = 1;
            int textX = blackX + (blackW - 6 * labelScale) / 2;
            int textY = rowY + blackH - 13;

            if (widget.isKeyPressed(keyDef.midiNote)) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
            }
            renderText(strBuf, textX, textY, labelScale);
        }
    }
}

PianoWidget::PianoWidget() {
    clearAllKeys();
}

void PianoWidget::clearAllKeys() {
    std::fill(std::begin(pressedKeys_), std::end(pressedKeys_), false);
}

void PianoWidget::setKeyPressed(int midiNote, bool pressed) {
    if (midiNote >= 0 && midiNote < 128) {
        pressedKeys_[midiNote] = pressed;
    }
}

bool PianoWidget::isKeyPressed(int midiNote) const {
    if (midiNote >= 0 && midiNote < 128) {
        return pressedKeys_[midiNote];
    }
    return false;
}

int PianoWidget::getMidiFromKey(SDL_Keycode sym, Uint16 mod) {
    bool shiftPressed = (mod & KMOD_SHIFT) != 0;

    if (!shiftPressed) {
        switch (sym) {
            // Bottom Right (Octave 2: G3..F4)
            case SDLK_y: return 55; // G3
            case SDLK_s: return 56; // G#3
            case SDLK_x: return 57; // A3
            case SDLK_d: return 58; // A#3
            case SDLK_c: return 59; // B3
            case SDLK_v: return 60; // C4 (Middle C)
            case SDLK_g: return 61; // C#4
            case SDLK_b: return 62; // D4
            case SDLK_h: return 63; // D#4
            case SDLK_n: return 64; // E4
            case SDLK_m: return 65; // F4
            case SDLK_j: return 66; // F#4

            // Top Left (Octave 3: G4..F5)
            case SDLK_q: return 67; // G4
            case SDLK_2: return 68; // G#4
            case SDLK_w: return 69; // A4
            case SDLK_3: return 70; // A#4
            case SDLK_e: return 71; // B4
            case SDLK_r: return 72; // C5
            case SDLK_5: return 73; // C#5
            case SDLK_t: return 74; // D5
            case SDLK_6: return 75; // D#5
            case SDLK_z: return 76; // E5
            case SDLK_u: return 77; // F5
            case SDLK_7: return 78; // F#5
            default: break;
        }
    } else {
        switch (sym) {
            // Bottom Left (Octave 1: G2..F3 - Lowest tone)
            case SDLK_y: return 43; // G2
            case SDLK_s: return 44; // G#2
            case SDLK_x: return 45; // A2
            case SDLK_d: return 46; // A#2
            case SDLK_c: return 47; // B2
            case SDLK_v: return 48; // C3
            case SDLK_g: return 49; // C#3
            case SDLK_b: return 50; // D3
            case SDLK_h: return 51; // D#3
            case SDLK_n: return 52; // E3
            case SDLK_m: return 53; // F3
            case SDLK_j: return 54; // F#3

            // Top Right (Octave 4: G5..F6 - Highest tone)
            case SDLK_q: return 79; // G5
            case SDLK_2: return 80; // G#5 (!)
            case SDLK_w: return 81; // A5
            case SDLK_3: return 82; // A#5 (")
            case SDLK_e: return 83; // B5
            case SDLK_r: return 84; // C6
            case SDLK_5: return 85; // C#6 ($)
            case SDLK_t: return 86; // D6
            case SDLK_6: return 87; // D#6 (&)
            case SDLK_z: return 88; // E6
            case SDLK_u: return 89; // F6
            case SDLK_7: return 90; // F#6 (/)
            default: break;
        }
    }
    return -1;
}

void PianoWidget::render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText) {
    // Header
    renderText("VIRTUAL PIANO KEYBOARD DIAGRAM (4 OCTAVES: G2 - F6)", x + 8, y + 4, 2);

    int topY = y + 22;
    int keyAreaHeight = height - 26;
    if (keyAreaHeight < 80) keyAreaHeight = 80;

    int rowHeight = (keyAreaHeight - 4) / 2;
    int topRowY = topY;
    int bottomRowY = topY + rowHeight + 4;

    int numWhite = 14;
    int whiteW = (width - 16) / numWhite;
    int startX = x + 8;

    // Render Top Row (Higher Octaves: G4 - F6)
    renderRow(renderer, startX, topRowY, whiteW, rowHeight, kTopWhiteKeys, 14, kTopBlackKeys, 10, *this, renderText);

    // Render Bottom Row (Lower Octaves: G2 - F4)
    renderRow(renderer, startX, bottomRowY, whiteW, rowHeight, kBottomWhiteKeys, 14, kBottomBlackKeys, 10, *this, renderText);
}
