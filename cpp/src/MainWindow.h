#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "SpectrumWidget.h"
#include "NoteDetectionWidget.h"
#include "NoteDetector.h"
#include "PentagramWidget.h"
#include "PianoWidget.h"
#include "MidiPlayer.h"
#include "AudioProcessor.h"
#include "Metronome.h"
#include <SDL.h>

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool init();
    void run();

private:
    void handleEvents();
    void render();
    void renderButton();
    void renderPlayButton();
    void renderSourcePanel();
    void renderStatus();
    void updateAudioSources();
    bool isVirtualKeyboardActive() const;
    void renderText(const char* text, int x, int y, int scale);
    void drawChar(char c, int x, int y, int scale);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SpectrumWidget spectrum_;
    NoteDetectionWidget noteWidget_;
    NoteDetector noteDetector_;
    PentagramWidget pentagramWidget_;
    PianoWidget pianoWidget_;
    MidiPlayer midiPlayer_;
    Metronome metronome_;
    std::vector<DetectedNote> detectedNotes_;
    std::vector<DetectedNote> virtualNotes_;
    Uint64 lastFrameTicks_ = 0;
    AudioProcessor audio_;
    bool running_ = false;
    bool audioAvailable_ = false;
    bool recording_ = false;
    int selectedSourceIndex_ = -1;
    std::vector<AudioProcessor::DeviceInfo> sourceDevices_;
    SDL_Rect buttonRect_ = {20, 20, 150, 45};
    SDL_Rect playButtonRect_ = {20, 75, 150, 45};
    SDL_Rect metronomeButtonRect_ = {20, 130, 150, 38};
    SDL_Rect bpmMinusRect_ = {20, 175, 40, 32};
    SDL_Rect bpmPlusRect_ = {130, 175, 40, 32};
    SDL_Rect timeSigButtonRect_ = {20, 215, 150, 38};
    SDL_Rect pauseBarsButtonRect_ = {20, 260, 150, 38};
    SDL_Rect resetButtonRect_ = {20, 305, 150, 38};
    SDL_Rect sourcePanelRect_ = {620, 20, 360, 260};
    SDL_Rect sourceItemRects_[16];
};

#endif // MAINWINDOW_H
