#include "NoteDetector.h"
#include <cmath>
#include <algorithm>
#include <set>

namespace {
    static const char* kNoteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
}

NoteDetector::NoteDetector() {
}

void NoteDetector::reset() {
    pitchTracker_.clear();
}

void NoteDetector::detectNotes(const AudioProcessor::SpectrumArray& spectrum, std::vector<DetectedNote>& outNotes, int maxNotes) {
    outNotes.clear();

    const int totalBins = static_cast<int>(spectrum.size());
    if (totalBins < 10) {
        pitchTracker_.clear();
        return;
    }

    // 1. Find overall maximum spectral magnitude
    float maxMag = 0.0f;
    for (float v : spectrum) {
        if (v > maxMag) maxMag = v;
    }

    if (maxMag < 0.05f) {
        // Silent audio stream: increment missingCount for tracked pitches
        for (auto it = pitchTracker_.begin(); it != pitchTracker_.end(); ) {
            it->second.missingCount++;
            if (it->second.missingCount > 3) { // 3 frames cutoff (~50ms)
                it = pitchTracker_.erase(it);
            } else {
                ++it;
            }
        }
        return;
    }

    // Dynamic noise thresholding (12% of maximum peak energy)
    float noiseThreshold = std::max(0.06f, maxMag * 0.12f);

    // 2. Extract prominent peak candidates in current audio frame
    std::unordered_map<int, DetectedNote> currentFramePeaks;
    const int win = 4;

    for (int i = win; i < totalBins - win; ++i) {
        float val = spectrum[i];
        if (val < noiseThreshold) continue;

        bool isMax = true;
        for (int w = -win; w <= win; ++w) {
            if (w != 0 && spectrum[i + w] > val) {
                isMax = false;
                break;
            }
        }
        if (!isMax) continue;

        // Peak Prominence
        float minLeft = val;
        for (int w = 1; w <= 12 && (i - w) >= 0; ++w) {
            minLeft = std::min(minLeft, spectrum[i - w]);
        }
        float minRight = val;
        for (int w = 1; w <= 12 && (i + w) < totalBins; ++w) {
            minRight = std::min(minRight, spectrum[i + w]);
        }
        float prominence = val - std::max(minLeft, minRight);

        if (prominence < 0.035f) continue;

        // Sub-bin Log-Gaussian peak centroid interpolation
        float y1 = std::max(1e-6f, spectrum[i - 1]);
        float y2 = std::max(1e-6f, spectrum[i]);
        float y3 = std::max(1e-6f, spectrum[i + 1]);

        float l1 = std::log(y1);
        float l2 = std::log(y2);
        float l3 = std::log(y3);

        float denom = l1 - 2.0f * l2 + l3;
        float delta = 0.0f;
        if (std::abs(denom) > 1e-6f) {
            delta = 0.5f * (l1 - l3) / denom;
            delta = std::clamp(delta, -0.5f, 0.5f);
        }

        float freq = static_cast<float>(AudioProcessor::kMinFrequency + i) + delta;
        if (freq < 20.0f || freq > 4200.0f) continue;

        float midiVal = 69.0f + 12.0f * std::log2(freq / 440.0f);
        int midiNote = static_cast<int>(std::round(midiVal));

        int noteIdx = ((midiNote % 12) + 12) % 12;
        int octave = (midiNote / 12) - 1;

        float exactFreq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
        float cents = 1200.0f * std::log2(freq / exactFreq);

        DetectedNote candidate;
        candidate.name = kNoteNames[noteIdx];
        candidate.octave = octave;
        candidate.frequency = freq;
        candidate.cents = cents;
        candidate.magnitude = val;
        candidate.midiNote = midiNote;

        auto it = currentFramePeaks.find(midiNote);
        if (it == currentFramePeaks.end() || val > it->second.magnitude) {
            currentFramePeaks[midiNote] = candidate;
        }
    }

    // 3. Update Temporal Pitch Tracker with Attack Settling Window (3-Frame Attack Delay)
    std::set<int> matchedMidis;

    for (const auto& kv : currentFramePeaks) {
        int midi = kv.first;
        float freq = kv.second.frequency;
        float mag = kv.second.magnitude;
        matchedMidis.insert(midi);

        auto it = pitchTracker_.find(midi);
        if (it != pitchTracker_.end()) {
            it->second.frameCount++;
            it->second.settlingFrames++;
            it->second.missingCount = 0;

            if (it->second.settlingFrames >= 3) {
                // Steady-state phase reached (attack transient passed!): Lock onto steady-state frequency
                if (!it->second.isLocked) {
                    it->second.frequency = freq; // Sample steady-state fundamental!
                    it->second.isLocked = true;
                } else {
                    // Smooth subtle steady-state pitch variations
                    it->second.frequency = 0.85f * it->second.frequency + 0.15f * freq;
                }
            } else {
                it->second.frequency = freq;
            }
            it->second.magnitude = 0.7f * it->second.magnitude + 0.3f * mag;
        } else {
            // New tone onset: start attack settling phase (settlingFrames = 1)
            pitchTracker_[midi] = { 1, 1, 0, freq, mag, false };
        }
    }

    // Increment missing count for pitches not present in current frame
    for (auto it = pitchTracker_.begin(); it != pitchTracker_.end(); ) {
        if (matchedMidis.find(it->first) == matchedMidis.end()) {
            it->second.missingCount++;
            if (it->second.missingCount > 3) { // 3 frames grace period (~50ms)
                it = pitchTracker_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }

    // 4. Emit Steady-State Locked Pitches (settlingFrames >= 3, ~50ms attack passed)
    std::vector<DetectedNote> stableNotes;

    for (const auto& kv : pitchTracker_) {
        int midi = kv.first;
        const auto& state = kv.second;

        // Emit notes that have passed the attack settling phase (>= 3 frames)
        if (state.settlingFrames >= 3) {
            int noteIdx = ((midi % 12) + 12) % 12;
            int octave = (midi / 12) - 1;

            float exactFreq = 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
            float cents = 1200.0f * std::log2(state.frequency / exactFreq);

            DetectedNote note;
            note.name = kNoteNames[noteIdx];
            note.octave = octave;
            note.frequency = state.frequency;
            note.cents = cents;
            note.magnitude = state.magnitude;
            note.midiNote = midi;

            stableNotes.push_back(note);
        }
    }

    // 5. Sort stable notes by magnitude descending and return top maxNotes
    std::sort(stableNotes.begin(), stableNotes.end(), [](const DetectedNote& a, const DetectedNote& b) {
        return a.magnitude > b.magnitude;
    });

    int count = std::min(static_cast<int>(stableNotes.size()), maxNotes);
    for (int k = 0; k < count; ++k) {
        outNotes.push_back(stableNotes[k]);
    }
}
