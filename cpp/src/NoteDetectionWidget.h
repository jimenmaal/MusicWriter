#ifndef NOTEDETECTIONWIDGET_H
#define NOTEDETECTIONWIDGET_H

#include "NoteDetector.h"
#include <SDL.h>
#include <vector>
#include <functional>

class NoteDetectionWidget {
public:
    using RenderTextFn = std::function<void(const char* text, int x, int y, int scale)>;

    NoteDetectionWidget();
    void setNotes(const std::vector<DetectedNote>& notes);
    void render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText);

private:
    std::vector<DetectedNote> notes_;
};

#endif // NOTEDETECTIONWIDGET_H
