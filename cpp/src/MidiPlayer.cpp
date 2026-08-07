#include "MidiPlayer.h"
#include <algorithm>

MidiPlayer::MidiPlayer() {
#if defined(_WIN32)
    midiOutOpen(&hMidiOut_, MIDIMAPPER, 0, 0, 0);
#endif
}

MidiPlayer::~MidiPlayer() {
    stopPlayback();
#if defined(_WIN32)
    if (hMidiOut_) {
        midiOutClose(hMidiOut_);
        hMidiOut_ = nullptr;
    }
#endif
}

void MidiPlayer::sendNoteOn(int midiNote, int velocity) {
#if defined(_WIN32)
    if (!hMidiOut_) {
        midiOutOpen(&hMidiOut_, MIDIMAPPER, 0, 0, 0);
    }
    if (hMidiOut_ && midiNote >= 0 && midiNote <= 127) {
        DWORD msg = 0x00000090 | ((midiNote & 0xFF) << 8) | ((velocity & 0xFF) << 16);
        midiOutShortMsg(hMidiOut_, msg);
    }
#endif
}

void MidiPlayer::sendNoteOff(int midiNote) {
#if defined(_WIN32)
    if (hMidiOut_ && midiNote >= 0 && midiNote <= 127) {
        DWORD msg = 0x00000080 | ((midiNote & 0xFF) << 8);
        midiOutShortMsg(hMidiOut_, msg);
    }
#endif
}

void MidiPlayer::silenceAll() {
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (i < noteStarted_.size() && noteStarted_[i] && (i >= noteEnded_.size() || !noteEnded_[i])) {
            sendNoteOff(notes_[i].midiNote);
        }
    }
#if defined(_WIN32)
    if (hMidiOut_) {
        // Control Change 123: All Notes Off
        DWORD msg = 0x00007B0B;
        midiOutShortMsg(hMidiOut_, msg);
    }
#endif
}

bool MidiPlayer::startPlayback(const std::vector<PentagramNote>& notes, float durationSeconds) {
    if (notes.empty()) return false;

#if defined(_WIN32)
    if (!hMidiOut_) {
        midiOutOpen(&hMidiOut_, MIDIMAPPER, 0, 0, 0);
    }
#endif

    notes_ = notes;
    std::sort(notes_.begin(), notes_.end(), [](const PentagramNote& a, const PentagramNote& b) {
        return a.timeStamp < b.timeStamp;
    });

    noteStarted_.assign(notes_.size(), false);
    noteEnded_.assign(notes_.size(), false);
    playbackTime_ = 0.0f;
    duration_ = durationSeconds;
    playing_ = true;

    return true;
}

void MidiPlayer::stopPlayback() {
    if (playing_) {
        silenceAll();
        playing_ = false;
    }
}

void MidiPlayer::update(float deltaTimeSeconds) {
    if (!playing_) return;

    playbackTime_ += deltaTimeSeconds;

    if (playbackTime_ >= duration_) {
        stopPlayback();
        return;
    }

    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& note = notes_[i];
        float noteStart = note.timeStamp;
        float noteEnd = note.timeStamp + std::max(0.12f, note.duration);

        // Send Note On when playback reaches start of note
        if (!noteStarted_[i] && playbackTime_ >= noteStart) {
            sendNoteOn(note.midiNote, 100);
            noteStarted_[i] = true;
        }

        // Send Note Off when playback reaches end of note duration
        if (noteStarted_[i] && !noteEnded_[i] && playbackTime_ >= noteEnd) {
            sendNoteOff(note.midiNote);
            noteEnded_[i] = true;
        }
    }
}
