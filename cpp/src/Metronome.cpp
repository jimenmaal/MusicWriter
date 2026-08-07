#include "Metronome.h"
#include <algorithm>
#include <thread>

Metronome::Metronome() {
#if defined(_WIN32)
    midiOutOpen(&hMidiOut_, MIDIMAPPER, 0, 0, 0);
#endif
}

Metronome::~Metronome() {
#if defined(_WIN32)
    if (hMidiOut_) {
        midiOutClose(hMidiOut_);
        hMidiOut_ = nullptr;
    }
#endif
}

void Metronome::setBpm(int bpm) {
    bpm_ = std::max(40, std::min(240, bpm));
}

void Metronome::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled_) {
        timeAccumulator_ = 0.0f;
        currentBeat_ = 0;
    }
}

void Metronome::reset() {
    timeAccumulator_ = 0.0f;
    currentBeat_ = 0;
}

void Metronome::update(float deltaTimeSeconds) {
    if (!enabled_) return;

    float qBeat = 60.0f / static_cast<float>(bpm_);
    float beatDuration = (timeSig_ == TimeSignature::TS_6_8) ? (qBeat * 0.5f) : qBeat;
    int maxBeats = getBeatsPerBar(timeSig_);

    timeAccumulator_ += deltaTimeSeconds;

    if (timeAccumulator_ >= beatDuration) {
        timeAccumulator_ -= beatDuration;
        bool isAccent = (currentBeat_ == 0);
        if (timeSig_ == TimeSignature::TS_6_8 && currentBeat_ == 3) {
            isAccent = true; // Compound accent on beat 4 in 6/8
        }
        playClick(isAccent);
        currentBeat_ = (currentBeat_ + 1) % maxBeats;
    }
}

void Metronome::playClick(bool isAccent) {
#if defined(_WIN32)
    if (!hMidiOut_) {
        midiOutOpen(&hMidiOut_, MIDIMAPPER, 0, 0, 0);
    }
    if (hMidiOut_) {
        int note = isAccent ? 76 : 77; // High Wood Block vs Low Wood Block on Percussion Channel
        int vel = isAccent ? 127 : 95;
        DWORD msg = 0x00000099 | ((note & 0xFF) << 8) | ((vel & 0xFF) << 16);
        midiOutShortMsg(hMidiOut_, msg);
    }
    std::thread([isAccent]() {
        DWORD freq = isAccent ? 1200 : 800;
        DWORD dur = isAccent ? 25 : 18;
        Beep(freq, dur);
    }).detach();
#endif
}
