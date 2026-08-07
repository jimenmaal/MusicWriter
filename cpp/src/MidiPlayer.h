#ifndef MIDIPLAYER_H
#define MIDIPLAYER_H

#include "PentagramWidget.h"
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <mmeapi.h>
#endif

class MidiPlayer {
public:
    MidiPlayer();
    ~MidiPlayer();

    bool startPlayback(const std::vector<PentagramNote>& notes, float durationSeconds = 20.0f);
    void stopPlayback();
    void update(float deltaTimeSeconds);
    bool isPlaying() const { return playing_; }
    float playbackTime() const { return playbackTime_; }

    void sendNoteOn(int midiNote, int velocity = 100);
    void sendNoteOff(int midiNote);

private:
    void silenceAll();

#if defined(_WIN32)
    HMIDIOUT hMidiOut_ = nullptr;
#endif

    bool playing_ = false;
    float playbackTime_ = 0.0f;
    float duration_ = 20.0f;
    std::vector<PentagramNote> notes_;
    std::vector<bool> noteStarted_;
    std::vector<bool> noteEnded_;
};

#endif // MIDIPLAYER_H
