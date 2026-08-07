#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <SDL.h>
#include <array>

class WaveformWidget {
public:
    static constexpr int kSampleCount = 1024;
    using WaveformArray = std::array<float, kSampleCount>;

    WaveformWidget();
    void render(SDL_Renderer* renderer, int x, int y, int width, int height);
    WaveformArray& data();

private:
    WaveformArray waveform_;
};

#endif // WAVEFORMWIDGET_H
