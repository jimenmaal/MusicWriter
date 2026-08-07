#include "NoteDetectionWidget.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

NoteDetectionWidget::NoteDetectionWidget() {
}

void NoteDetectionWidget::setNotes(const std::vector<DetectedNote>& notes) {
    notes_ = notes;
}

void NoteDetectionWidget::render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText) {
    // 1. Panel Title
    renderText("DETECTED FUNDAMENTAL NOTES", x + 8, y + 8, 2);

    // 2. 12-Semitone Pitch Class Strip at panel bottom
    static const char* pitchClasses[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    bool activePitchClasses[12] = {false};
    for (const auto& note : notes_) {
        int idx = ((note.midiNote % 12) + 12) % 12;
        activePitchClasses[idx] = true;
    }

    int keyBarY = y + height - 36;
    int keyBarWidth = width - 16;
    int keyWidth = (keyBarWidth - 11 * 2) / 12;

    for (int i = 0; i < 12; ++i) {
        int keyX = x + 8 + i * (keyWidth + 2);
        SDL_Rect keyRect = { keyX, keyBarY, keyWidth, 26 };

        if (activePitchClasses[i]) {
            SDL_SetRenderDrawColor(renderer, 0, 180, 240, 255);
            SDL_RenderFillRect(renderer, &keyRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &keyRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 35, 35, 35, 255);
            SDL_RenderFillRect(renderer, &keyRect);
            SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
            SDL_RenderDrawRect(renderer, &keyRect);
        }

        int textLen = static_cast<int>(std::strlen(pitchClasses[i]));
        int textX = keyX + (keyWidth - textLen * 6) / 2;
        if (textX < keyX) textX = keyX;
        renderText(pitchClasses[i], textX, keyBarY + 7, 1);
    }

    // 3. Render detected note cards
    int cardAreaY = y + 32;
    int cardAreaHeight = keyBarY - cardAreaY - 8;

    if (notes_.empty()) {
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
        renderText("NO NOTES DETECTED", x + 16, cardAreaY + cardAreaHeight / 2 - 8, 2);
        return;
    }

    int count = static_cast<int>(notes_.size());
    int cardWidth = (width - 16 - (count - 1) * 8) / count;
    if (cardWidth < 70) cardWidth = 70;

    for (int i = 0; i < count; ++i) {
        const auto& note = notes_[i];
        int cardX = x + 8 + i * (cardWidth + 8);
        SDL_Rect cardRect = { cardX, cardAreaY, cardWidth, cardAreaHeight };

        // Card background & border
        SDL_SetRenderDrawColor(renderer, 24, 38, 54, 255);
        SDL_RenderFillRect(renderer, &cardRect);
        SDL_SetRenderDrawColor(renderer, 0, 160, 230, 255);
        SDL_RenderDrawRect(renderer, &cardRect);

        // Note Name (Large font scale)
        char noteLabel[16];
        std::snprintf(noteLabel, sizeof(noteLabel), "%s%d", note.name.c_str(), note.octave);
        renderText(noteLabel, cardX + 10, cardAreaY + 8, 3);

        // Frequency (Hz)
        char freqLabel[32];
        std::snprintf(freqLabel, sizeof(freqLabel), "%.1f HZ", note.frequency);
        renderText(freqLabel, cardX + 10, cardAreaY + 36, 1);

        // Cents detuning
        char centsLabel[32];
        if (note.cents >= 0.0f) {
            std::snprintf(centsLabel, sizeof(centsLabel), "+%.1f C", note.cents);
        } else {
            std::snprintf(centsLabel, sizeof(centsLabel), "%.1f C", note.cents);
        }
        renderText(centsLabel, cardX + 10, cardAreaY + 50, 1);

        // Magnitude meter at bottom of card
        int meterY = cardAreaY + cardAreaHeight - 12;
        int meterW = cardWidth - 20;
        if (meterW > 0) {
            SDL_Rect meterBg = { cardX + 10, meterY, meterW, 5 };
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &meterBg);

            int filledW = static_cast<int>(meterW * std::clamp(note.magnitude, 0.0f, 1.0f));
            if (filledW > 0) {
                SDL_Rect meterFg = { cardX + 10, meterY, filledW, 5 };
                SDL_SetRenderDrawColor(renderer, 0, 230, 120, 255);
                SDL_RenderFillRect(renderer, &meterFg);
            }
        }
    }
}
