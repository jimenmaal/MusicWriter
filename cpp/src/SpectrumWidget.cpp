#include "SpectrumWidget.h"
#include <cmath>

SpectrumWidget::SpectrumWidget() {
    spectrum_.fill(0.0f);
}

SpectrumWidget::SpectrumArray& SpectrumWidget::data() {
    return spectrum_;
}

static void selectGradientColor(float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (t <= 0.33f) {
        float ratio = t / 0.33f;
        r = 255;
        g = static_cast<uint8_t>(ratio * 255);
        b = 0;
    } else if (t <= 0.66f) {
        float ratio = (t - 0.33f) / 0.33f;
        r = static_cast<uint8_t>(255 - ratio * 255);
        g = 255;
        b = 0;
    } else {
        float ratio = (t - 0.66f) / 0.34f;
        r = 0;
        g = static_cast<uint8_t>(255 - ratio * 255);
        b = static_cast<uint8_t>(ratio * 255);
    }
}

void SpectrumWidget::render(SDL_Renderer* renderer, int x, int y, int width, int height) {
    const int binCount = static_cast<int>(spectrum_.size());
    if (binCount == 0 || width <= 0 || height <= 0) {
        return;
    }

    int visibleBars = width;
    if (visibleBars > binCount) {
        visibleBars = binCount;
    }

    float binsPerBar = static_cast<float>(binCount) / static_cast<float>(visibleBars);
    int barWidth = width / visibleBars;
    if (barWidth < 1) barWidth = 1;

    for (int i = 0; i < visibleBars; ++i) {
        int startBin = static_cast<int>(std::floor(i * binsPerBar));
        int endBin = static_cast<int>(std::floor((i + 1) * binsPerBar));
        if (endBin <= startBin) {
            endBin = startBin + 1;
        }
        if (startBin >= binCount) {
            break;
        }
        if (endBin > binCount) {
            endBin = binCount;
        }

        float value = 0.0f;
        for (int bin = startBin; bin < endBin; ++bin) {
            value = std::max(value, spectrum_[bin]);
        }
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        int barHeight = static_cast<int>(value * height);

        float t = visibleBars > 1 ? static_cast<float>(i) / static_cast<float>(visibleBars - 1) : 0.0f;
        uint8_t r, g, b;
        selectGradientColor(t, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);

        int rectWidth = barWidth > 1 ? barWidth - 1 : 1;
        SDL_Rect rect = { x + i * barWidth, y + (height - barHeight), rectWidth, barHeight };
        SDL_RenderFillRect(renderer, &rect);
    }
}
