#include "WaveformWidget.h"

WaveformWidget::WaveformWidget() {
    waveform_.fill(0.0f);
}

WaveformWidget::WaveformArray& WaveformWidget::data() {
    return waveform_;
}

void WaveformWidget::render(SDL_Renderer* renderer, int x, int y, int width, int height) {
    SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
    SDL_Rect background = {x, y, width, height};
    SDL_RenderFillRect(renderer, &background);
    SDL_SetRenderDrawColor(renderer, 76, 175, 80, 255);

    int centerY = y + height / 2;
    int sampleStep = static_cast<int>(WaveformWidget::kSampleCount / width);
    if (sampleStep < 1) sampleStep = 1;

    int prevX = x;
    int prevY = centerY;
    for (int i = 0; i < width; ++i) {
        int sampleIndex = i * sampleStep;
        if (sampleIndex >= kSampleCount) {
            sampleIndex = kSampleCount - 1;
        }
        float amplitude = waveform_[sampleIndex];
        if (amplitude < -1.0f) amplitude = -1.0f;
        if (amplitude > 1.0f) amplitude = 1.0f;
        int currentY = centerY - static_cast<int>(amplitude * (height * 0.45f));
        SDL_RenderDrawLine(renderer, prevX, prevY, x + i, currentY);
        prevX = x + i;
        prevY = currentY;
    }

    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawLine(renderer, x, centerY, x + width, centerY);
}
