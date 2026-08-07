#ifndef NOTEDETECTOR_H
#define NOTEDETECTOR_H

#include "AudioProcessor.h"
#include <vector>
#include <string>
#include <unordered_map>

struct DetectedNote {
    std::string name;       // e.g. "A", "C#"
    int octave = 4;         // e.g. 4 for A4
    float frequency = 0.0f; // e.g. 440.0 Hz
    float cents = 0.0f;     // detuning in cents
    float magnitude = 0.0f; // magnitude in range [0..1]
    int midiNote = 69;      // MIDI note index
};

struct TrackedPitchState {
    int frameCount = 0;
    int settlingFrames = 0;
    int missingCount = 0;
    float frequency = 0.0f;
    float magnitude = 0.0f;
    bool isLocked = false;
};

class NoteDetector {
public:
    NoteDetector();
    void detectNotes(const AudioProcessor::SpectrumArray& spectrum, std::vector<DetectedNote>& outNotes, int maxNotes = 2);
    void reset();

private:
    std::unordered_map<int, TrackedPitchState> pitchTracker_;
};

#endif // NOTEDETECTOR_H
