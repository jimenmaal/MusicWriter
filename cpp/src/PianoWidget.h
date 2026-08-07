#ifndef PIANOWIDGET_H
#define PIANOWIDGET_H

#include <SDL.h>
#include <vector>
#include <string>
#include <functional>

struct PianoKeyMapping {
    char keyChar;           // Character label displayed on key ('a', 'w', 's', 'e', etc.)
    int midiNote;           // MIDI note number (60 = C4, 83 = B5)
    bool isBlack;           // True for sharp/flat black keys
    std::string noteName;   // e.g. "C4", "C#4"
};

class PianoWidget {
public:
    using RenderTextFn = std::function<void(const char* text, int x, int y, int scale)>;

    PianoWidget();
    void setKeyPressed(int midiNote, bool pressed);
    void clearAllKeys();
    bool isKeyPressed(int midiNote) const;
    void render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText);

    static int getMidiFromKey(SDL_Keycode sym, Uint16 mod);

private:
    bool pressedKeys_[128] = {false};
};

#endif // PIANOWIDGET_H
