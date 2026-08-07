#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include "AudioProcessor.h"
#include <SDL.h>

class SpectrumWidget {
public:
    static constexpr int kBinCount = AudioProcessor::kBinCount;
    using SpectrumArray = AudioProcessor::SpectrumArray;

    SpectrumWidget();
    void render(SDL_Renderer* renderer, int x, int y, int width, int height);
    SpectrumArray& data();

private:
    SpectrumArray spectrum_;
};

#endif // SPECTRUMWIDGET_H
