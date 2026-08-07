#ifndef PENTAGRAMWIDGET_H
#define PENTAGRAMWIDGET_H

#include "NoteDetector.h"
#include "Metronome.h"
#include <SDL.h>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <set>

struct PentagramNote {
    float xRatio = 0.0f;      // Start position [0..1] over 20 seconds
    float endXRatio = 0.0f;   // End position [0..1] over 20 seconds
    int midiNote = 60;
    std::string name;
    int octave = 4;
    bool isAccidental = false;
    bool isFlat = false;       // False for Sharp (#), True for Bemol/Flat (b)
    int diatonicStep = 0;     // Step relative to C4 (0 = C4, 2 = E4, 10 = F5, -2 = A3, -10 = G2)
    float timeStamp = 0.0f;   // Start time in seconds [0..20.0]
    float duration = 0.0f;    // Duration in seconds
    float frequency = 0.0f;   // Fundamental frequency in Hz
    bool isActive = true;
    float lastActiveTime = 0.0f;
};

struct PentagramRest {
    float xRatio = 0.0f;
    float endXRatio = 0.0f;
    float timeStamp = 0.0f;
    float duration = 0.0f;
};

class PentagramWidget {
public:
    using RenderTextFn = std::function<void(const char* text, int x, int y, int scale)>;

    PentagramWidget();
    void update(const std::vector<DetectedNote>& activeNotes, float deltaTimeSeconds, bool isRecording);
    void setPlaybackCursor(float timeSeconds);
    void setBpm(int bpm) { bpm_ = std::max(40, std::min(240, bpm)); }
    int bpm() const { return bpm_; }
    void setTimeSignature(TimeSignature ts) { timeSig_ = ts; reset(); }
    TimeSignature timeSignature() const { return timeSig_; }
    void setPauseBetweenBars(bool enabled) { pauseBetweenBars_ = enabled; if (!enabled) isBarPaused_ = false; }
    bool pauseBetweenBars() const { return pauseBetweenBars_; }
    bool isBarPaused() const { return isBarPaused_; }
    const std::vector<PentagramNote>& recordedNotes() const { return recordedNotes_; }
    const std::vector<PentagramRest>& recordedRests() const { return recordedRests_; }
    void reset();
    void render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText);

private:
    float currentTime_ = 0.0f; // Current sweep time in seconds [0..20.0]
    int bpm_ = 120;            // Metronome BPM for measure bars & note values
    TimeSignature timeSig_ = TimeSignature::TS_4_4;
    bool pauseBetweenBars_ = false;
    bool isBarPaused_ = false;
    int lastMidiNote_ = -1;
    std::vector<PentagramNote> recordedNotes_;
    std::vector<PentagramRest> recordedRests_;
    std::unordered_map<int, size_t> activeNoteIndices_;
    size_t currentRestIndex_ = static_cast<size_t>(-1);
};

#endif // PENTAGRAMWIDGET_H
