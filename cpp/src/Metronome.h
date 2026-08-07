#ifndef METRONOME_H
#define METRONOME_H

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <mmeapi.h>
#endif

enum class TimeSignature {
    TS_4_4,
    TS_3_4,
    TS_2_4,
    TS_6_8
};

inline int getBeatsPerBar(TimeSignature ts) {
    switch (ts) {
        case TimeSignature::TS_3_4: return 3;
        case TimeSignature::TS_2_4: return 2;
        case TimeSignature::TS_6_8: return 6;
        case TimeSignature::TS_4_4:
        default: return 4;
    }
}

inline const char* getTimeSignatureLabel(TimeSignature ts) {
    switch (ts) {
        case TimeSignature::TS_3_4: return "3/4";
        case TimeSignature::TS_2_4: return "2/4";
        case TimeSignature::TS_6_8: return "6/8";
        case TimeSignature::TS_4_4:
        default: return "4/4";
    }
}

class Metronome {
public:
    Metronome();
    ~Metronome();

    void setBpm(int bpm);
    int bpm() const { return bpm_; }

    void setTimeSignature(TimeSignature ts) { timeSig_ = ts; reset(); }
    TimeSignature timeSignature() const { return timeSig_; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    void reset();
    void update(float deltaTimeSeconds);

    int currentBeat() const { return currentBeat_; }

private:
    void playClick(bool isAccent);

    int bpm_ = 120;
    TimeSignature timeSig_ = TimeSignature::TS_4_4;
    bool enabled_ = false;
    float timeAccumulator_ = 0.0f;
    int currentBeat_ = 0;

#if defined(_WIN32)
    HMIDIOUT hMidiOut_ = nullptr;
#endif
};

#endif // METRONOME_H
