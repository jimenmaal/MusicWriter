#include "PentagramWidget.h"
#include <algorithm>
#include <cmath>

namespace {
    static const int kDiatonicStepSharp[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    static const int kDiatonicStepFlat[12]  = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    static const bool kIsAccidental[12]     = { false, true, false, true, false, false, true, false, true, false, true, false };
    static const char* kSharpNames[12]      = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* kFlatNames[12]       = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                }
            }
        }
    }
}

PentagramWidget::PentagramWidget() {
}

void PentagramWidget::reset() {
    currentTime_ = 0.0f;
    isBarPaused_ = false;
    lastMidiNote_ = -1;
    recordedNotes_.clear();
    recordedRests_.clear();
    currentRestIndex_ = static_cast<size_t>(-1);
}

void PentagramWidget::setPlaybackCursor(float timeSeconds) {
    currentTime_ = std::clamp(timeSeconds, 0.0f, 20.0f);
}

void PentagramWidget::update(const std::vector<DetectedNote>& activeNotes, float deltaTimeSeconds, bool isRecording) {
    if (!isRecording) {
        for (auto& n : recordedNotes_) n.isActive = false;
        currentRestIndex_ = static_cast<size_t>(-1);
        isBarPaused_ = false;
        return;
    }

    if (isBarPaused_) {
        if (!activeNotes.empty()) {
            isBarPaused_ = false; // Resume recording when next tone is detected!
            currentRestIndex_ = static_cast<size_t>(-1);
        } else {
            return; // Pause at bar line boundary until note is detected!
        }
    }

    float prevTime = currentTime_;
    currentTime_ += deltaTimeSeconds;

    float qBeat = 60.0f / static_cast<float>(bpm_);
    float secondsPerBeat = (timeSig_ == TimeSignature::TS_6_8) ? (qBeat * 0.5f) : qBeat;
    float secondsPerBar = getBeatsPerBar(timeSig_) * secondsPerBeat;

    // Check Pause Between Bars boundary
    if (pauseBetweenBars_ && activeNotes.empty()) {
        int prevBar = static_cast<int>(prevTime / secondsPerBar);
        int curBar = static_cast<int>(currentTime_ / secondsPerBar);
        if (curBar > prevBar && curBar <= static_cast<int>(20.0f / secondsPerBar)) {
            currentTime_ = curBar * secondsPerBar;
            isBarPaused_ = true;
        }
    }

    if (currentTime_ >= 20.0f) {
        currentTime_ -= 20.0f;
        for (auto& n : recordedNotes_) n.isActive = false;
        currentRestIndex_ = static_cast<size_t>(-1);
    }

    // Clear old notes and rests directly ahead of sweeping cursor (within a 0.4s window in front of currentTime_)
    float clearWindowEnd = currentTime_ + 0.4f;
    recordedNotes_.erase(
        std::remove_if(recordedNotes_.begin(), recordedNotes_.end(), [this, clearWindowEnd](const PentagramNote& n) {
            if (clearWindowEnd < 20.0f) {
                return n.timeStamp > (currentTime_ + 0.05f) && n.timeStamp <= clearWindowEnd;
            } else {
                return n.timeStamp > (currentTime_ + 0.05f) || n.timeStamp <= (clearWindowEnd - 20.0f);
            }
        }),
        recordedNotes_.end()
    );
    recordedRests_.erase(
        std::remove_if(recordedRests_.begin(), recordedRests_.end(), [this, clearWindowEnd](const PentagramRest& r) {
            if (clearWindowEnd < 20.0f) {
                return r.timeStamp > (currentTime_ + 0.05f) && r.timeStamp <= clearWindowEnd;
            } else {
                return r.timeStamp > (currentTime_ + 0.05f) || r.timeStamp <= (clearWindowEnd - 20.0f);
            }
        }),
        recordedRests_.end()
    );

    // If no notes detected: deactivate ongoing notes after 0.08s grace period
    if (activeNotes.empty()) {
        for (auto& n : recordedNotes_) {
            if (n.isActive && (currentTime_ - n.lastActiveTime > 0.08f)) {
                n.isActive = false;
            }
        }

        // Update ongoing rest or start a new rest interval
        if (currentRestIndex_ < recordedRests_.size()) {
            auto& rest = recordedRests_[currentRestIndex_];
            rest.duration = currentTime_ - rest.timeStamp;
            rest.endXRatio = currentTime_ / 20.0f;
        } else {
            PentagramRest rest;
            rest.xRatio = currentTime_ / 20.0f;
            rest.endXRatio = currentTime_ / 20.0f;
            rest.timeStamp = currentTime_;
            rest.duration = 0.0f;
            recordedRests_.push_back(rest);
            currentRestIndex_ = recordedRests_.size() - 1;
        }
        return;
    }

    currentRestIndex_ = static_cast<size_t>(-1);

    // Process all active notes concurrently (Polyphonic & Frequency Tolerance matching)
    for (const auto& activeNote : activeNotes) {
        int midi = activeNote.midiNote;
        float freq = activeNote.frequency;

        // Search backward for matching ongoing note in recordedNotes_
        PentagramNote* matchedNote = nullptr;
        for (int i = static_cast<int>(recordedNotes_.size()) - 1; i >= 0; --i) {
            auto& candidate = recordedNotes_[i];
            bool timeRecent = candidate.isActive &&
                              (candidate.timeStamp <= currentTime_) &&
                              (currentTime_ >= candidate.lastActiveTime) &&
                              (currentTime_ - candidate.lastActiveTime <= 0.08f);
            if (!timeRecent) continue;

            bool isMatch = false;
            if (midi >= 0 && candidate.midiNote == midi) {
                isMatch = true;
            } else if (freq > 0.0f && candidate.frequency > 0.0f) {
                float relDiff = std::abs(freq - candidate.frequency) / candidate.frequency;
                if (relDiff <= 0.10f) {
                    isMatch = true;
                }
            }

            if (isMatch) {
                matchedNote = &candidate;
                break;
            }
        }

        if (matchedNote) {
            matchedNote->isActive = true;
            matchedNote->lastActiveTime = currentTime_;
            matchedNote->duration = currentTime_ - matchedNote->timeStamp;
            matchedNote->endXRatio = currentTime_ / 20.0f;
        } else {
            // Determine accidental spelling based on melodic direction (Descending -> Flat/Bemol, Ascending -> Sharp)
            bool isFlat = false;
            if (lastMidiNote_ >= 0 && midi < lastMidiNote_) {
                isFlat = true;
            }
            lastMidiNote_ = midi;

            int pitchClass = ((midi % 12) + 12) % 12;
            int octave = (midi / 12) - 1;
            int stepInOct = isFlat ? kDiatonicStepFlat[pitchClass] : kDiatonicStepSharp[pitchClass];
            int diatonicStep = (octave - 4) * 7 + stepInOct;

            PentagramNote note;
            note.xRatio = currentTime_ / 20.0f;
            note.endXRatio = currentTime_ / 20.0f;
            note.midiNote = midi;
            note.name = isFlat ? kFlatNames[pitchClass] : kSharpNames[pitchClass];
            note.octave = octave;
            note.isAccidental = kIsAccidental[pitchClass];
            note.isFlat = isFlat;
            note.diatonicStep = diatonicStep;
            note.timeStamp = currentTime_;
            note.duration = 0.0f;
            note.frequency = freq;
            note.isActive = true;
            note.lastActiveTime = currentTime_;

            recordedNotes_.push_back(note);
        }
    }

    // Inactivate notes that were not present in activeNotes and passed the grace period
    for (auto& n : recordedNotes_) {
        if (n.isActive && (currentTime_ - n.lastActiveTime > 0.08f)) {
            n.isActive = false;
        }
    }
}

namespace {
    void drawNoteHead(SDL_Renderer* renderer, int cx, int cy, bool isFilled) {
        int rx = 5;
        int ry = 4;
        for (int dy = -ry; dy <= ry; ++dy) {
            for (int dx = -rx; dx <= rx; ++dx) {
                float distSq = (static_cast<float>(dx * dx) / (rx * rx)) + (static_cast<float>(dy * dy) / (ry * ry));
                if (isFilled) {
                    if (distSq <= 1.0f) {
                        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                    }
                } else {
                    if (distSq <= 1.0f && distSq >= 0.45f) {
                        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                    }
                }
            }
        }
    }

    void drawAugmentationDot(SDL_Renderer* renderer, int cx, int cy, int diatonicStep) {
        // If note sits on a line (even diatonicStep), place dot in space above
        int dotY = (diatonicStep % 2 == 0) ? (cy - 2) : cy;
        int dotX = cx + 9;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                SDL_RenderDrawPoint(renderer, dotX + dx, dotY + dy);
            }
        }
    }

    void drawTieArc(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, bool isStemDown) {
        int midX = (x1 + x2) / 2;
        int arcHeight = 12;
        int controlY = isStemDown ? (y1 - arcHeight) : (y1 + arcHeight);

        int prevX = x1;
        int prevY = y1;
        int steps = 16;
        for (int i = 1; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float invT = 1.0f - t;
            int curX = static_cast<int>(invT * invT * x1 + 2.0f * invT * t * midX + t * t * x2);
            int curY = static_cast<int>(invT * invT * y1 + 2.0f * invT * t * controlY + t * t * y2);

            SDL_RenderDrawLine(renderer, prevX, prevY, curX, curY);
            SDL_RenderDrawLine(renderer, prevX, prevY + 1, curX, curY + 1);
            prevX = curX;
            prevY = curY;
        }
    }

    void drawRestSymbol(SDL_Renderer* renderer, int rx, int ry, float beatRatio) {
        enum class RestType {
            Semibreve, DottedSemibreve,
            Minim, DottedMinim,
            Crotchet, DottedCrotchet,
            Quaver, DottedQuaver,
            Semiquaver, DottedSemiquaver
        };

        RestType restType = RestType::Semiquaver;
        if (beatRatio >= 16.0f) restType = RestType::Semibreve;
        else if (beatRatio >= 12.0f) restType = RestType::DottedMinim;
        else if (beatRatio >= 6.0f) restType = RestType::Minim;
        else if (beatRatio >= 3.0f) restType = RestType::DottedCrotchet;
        else if (beatRatio >= 1.5f) restType = RestType::Crotchet;
        else if (beatRatio >= 0.75f) restType = RestType::DottedQuaver;
        else if (beatRatio >= 0.375f) restType = RestType::Quaver;
        else if (beatRatio >= 0.1875f) restType = RestType::DottedSemiquaver;

        bool hasDot = (restType == RestType::DottedSemiquaver || restType == RestType::DottedQuaver ||
                       restType == RestType::DottedCrotchet || restType == RestType::DottedMinim);

        SDL_SetRenderDrawColor(renderer, 180, 200, 230, 255);

        if (restType == RestType::Semibreve || restType == RestType::DottedSemibreve) {
            // Whole Rest: Solid rectangle hanging below line 4
            SDL_Rect rect = { rx - 5, ry - 4, 10, 5 };
            SDL_RenderFillRect(renderer, &rect);
        } else if (restType == RestType::Minim || restType == RestType::DottedMinim) {
            // Half Rest: Solid rectangle sitting on line 3
            SDL_Rect rect = { rx - 5, ry - 1, 10, 5 };
            SDL_RenderFillRect(renderer, &rect);
        } else if (restType == RestType::Crotchet || restType == RestType::DottedCrotchet) {
            // Quarter Rest: Zigzag symbol
            SDL_RenderDrawLine(renderer, rx - 3, ry - 8, rx + 3, ry - 3);
            SDL_RenderDrawLine(renderer, rx + 3, ry - 3, rx - 3, ry + 2);
            SDL_RenderDrawLine(renderer, rx - 3, ry + 2, rx + 2, ry + 7);
            SDL_RenderDrawLine(renderer, rx + 2, ry + 7, rx - 1, ry + 9);
        } else {
            // Eighth (1 hook) or Sixteenth (2 hooks) Rest
            int flagCount = (restType == RestType::Semiquaver || restType == RestType::DottedSemiquaver) ? 2 : 1;
            SDL_RenderDrawLine(renderer, rx + 2, ry - 8, rx - 4, ry + 8);
            for (int f = 0; f < flagCount; ++f) {
                int hy = ry - 6 + f * 5;
                SDL_RenderDrawLine(renderer, rx + 2, hy, rx - 3, hy - 3);
                SDL_RenderDrawLine(renderer, rx - 3, hy - 3, rx - 2, hy);
            }
        }

        if (hasDot) {
            SDL_Rect dot = { rx + 8, ry - 1, 3, 3 };
            SDL_RenderFillRect(renderer, &dot);
        }
    }
}

void PentagramWidget::render(SDL_Renderer* renderer, int x, int y, int width, int height, RenderTextFn renderText) {
    // 1. Panel Header
    std::string headerStr = "GRAND STAFF PENTAGRAM (" + std::string(getTimeSignatureLabel(timeSig_)) + " TIME, " + std::to_string(bpm_) + " BPM)";
    renderText(headerStr.c_str(), x + 8, y + 6, 2);

    int marginLeft = 65;
    int marginRight = 20;
    int staffWidth = width - marginLeft - marginRight;
    if (staffWidth < 100) staffWidth = 100;

    // Grand Staff spacing math:
    float lineSpacing = std::max(5.0f, static_cast<float>(height - 45) / 12.0f);
    float middleCY = y + 28.0f + 5.5f * lineSpacing;

    int trebleTopY = static_cast<int>(middleCY - 10 * 0.5f * lineSpacing);
    int trebleBottomY = static_cast<int>(middleCY - 2 * 0.5f * lineSpacing);
    int bassTopY = static_cast<int>(middleCY - (-2) * 0.5f * lineSpacing);
    int bassBottomY = static_cast<int>(middleCY - (-10) * 0.5f * lineSpacing);

    // Treble Staff Lines (Line 1 E4 at S=2 up to Line 5 F5 at S=10)
    SDL_SetRenderDrawColor(renderer, 180, 195, 215, 255);
    for (int i = 0; i < 5; ++i) {
        int diatonicS = 2 + i * 2; // 2, 4, 6, 8, 10
        int lineY = static_cast<int>(middleCY - diatonicS * 0.5f * lineSpacing);
        SDL_RenderDrawLine(renderer, x + marginLeft, lineY, x + marginLeft + staffWidth, lineY);
    }

    // Bass Staff Lines (Line 1 G2 at S=-10 up to Line 5 A3 at S=-2)
    SDL_SetRenderDrawColor(renderer, 160, 175, 195, 255);
    for (int i = 0; i < 5; ++i) {
        int diatonicS = -10 + i * 2; // -10, -8, -6, -4, -2
        int lineY = static_cast<int>(middleCY - diatonicS * 0.5f * lineSpacing);
        SDL_RenderDrawLine(renderer, x + marginLeft, lineY, x + marginLeft + staffWidth, lineY);
    }

    // Clef & Time Signature Labels
    int trebleCenterY = static_cast<int>(middleCY - 6.0f * 0.5f * lineSpacing);
    int bassCenterY = static_cast<int>(middleCY - (-6.0f) * 0.5f * lineSpacing);

    renderText("TREBLE", x + 4, trebleCenterY - 4, 1);
    renderText("BASS", x + 10, bassCenterY - 4, 1);

    // Time Signature Symbols at the start of the staves (4/4, 3/4, 2/4, 6/8)
    SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
    const char* topNum = (timeSig_ == TimeSignature::TS_3_4) ? "3" : ((timeSig_ == TimeSignature::TS_2_4) ? "2" : ((timeSig_ == TimeSignature::TS_6_8) ? "6" : "4"));
    const char* botNum = (timeSig_ == TimeSignature::TS_6_8) ? "8" : "4";

    renderText(topNum, x + marginLeft + 4, trebleCenterY - 8, 1);
    renderText(botNum, x + marginLeft + 4, trebleCenterY + 2, 1);
    renderText(topNum, x + marginLeft + 4, bassCenterY - 8, 1);
    renderText(botNum, x + marginLeft + 4, bassCenterY + 2, 1);

    // Vertical Measure Bar Lines
    float qBeat = 60.0f / static_cast<float>(bpm_);
    float secondsPerBeat = (timeSig_ == TimeSignature::TS_6_8) ? (qBeat * 0.5f) : qBeat;
    float secondsPerBar = getBeatsPerBar(timeSig_) * secondsPerBeat;
    int totalBars = static_cast<int>(20.0f / secondsPerBar);

    SDL_SetRenderDrawColor(renderer, 110, 130, 155, 255);
    for (int b = 1; b <= totalBars; ++b) {
        float barTime = b * secondsPerBar;
        if (barTime > 20.0f) break;
        float barRatio = barTime / 20.0f;
        int barX = x + marginLeft + static_cast<int>(barRatio * staffWidth);

        // Draw bar line through Treble and Bass staves
        SDL_RenderDrawLine(renderer, barX, trebleTopY, barX, trebleBottomY);
        SDL_RenderDrawLine(renderer, barX, bassTopY, barX, bassBottomY);

        // Render Bar Number Label
        std::string barLabel = "B" + std::to_string(b + 1);
        renderText(barLabel.c_str(), barX - 6, trebleTopY - 10, 1);
    }

    // Double Bar Line at the end of the pentagram (20s)
    int endX = x + marginLeft + staffWidth;
    SDL_RenderDrawLine(renderer, endX - 3, trebleTopY, endX - 3, bassBottomY);
    SDL_RenderDrawLine(renderer, endX, trebleTopY, endX, bassBottomY);

    // Draw Recorded Rests (Silences between notes)
    for (const auto& rest : recordedRests_) {
        int restX = x + marginLeft + static_cast<int>(rest.xRatio * staffWidth);
        int restX2 = x + marginLeft + static_cast<int>(rest.endXRatio * staffWidth);
        if (restX2 < restX) restX2 = restX;

        float durationSec = rest.duration;
        if (durationSec <= 0.01f && (restX2 > restX)) {
            durationSec = (rest.endXRatio - rest.xRatio) * 20.0f;
        }
        float beatRatio = durationSec / secondsPerBeat;

        // Render Rest Symbol on Treble staff (B4 line)
        int trebleRestY = static_cast<int>(middleCY - 6.0f * 0.5f * lineSpacing);
        drawRestSymbol(renderer, restX, trebleRestY, beatRatio);

        // Render Rest Symbol on Bass staff (D3 line)
        int bassRestY = static_cast<int>(middleCY - (-6.0f) * 0.5f * lineSpacing);
        drawRestSymbol(renderer, restX, bassRestY, beatRatio);
    }

    // Draw Recorded Notes with Dotted Note Morphing and Measure Bar Ties
    for (const auto& note : recordedNotes_) {
        int noteX1 = x + marginLeft + static_cast<int>(note.xRatio * staffWidth);
        int noteX2 = x + marginLeft + static_cast<int>(note.endXRatio * staffWidth);
        if (noteX2 < noteX1) noteX2 = noteX1;

        int noteY = static_cast<int>(middleCY - note.diatonicStep * 0.5f * lineSpacing);

        // Ledger Lines
        SDL_SetRenderDrawColor(renderer, 220, 220, 240, 255);
        int lx1 = noteX1 - 8;
        int lx2 = noteX1 + 8;

        if (note.diatonicStep == 0) { // Middle C (C4)
            int ledgerY = static_cast<int>(middleCY);
            SDL_RenderDrawLine(renderer, lx1, ledgerY, lx2, ledgerY);
        } else if (note.diatonicStep < -10) { // Below Bass Staff
            for (int step = -12; step >= note.diatonicStep; step -= 2) {
                int ledgerY = static_cast<int>(middleCY - step * 0.5f * lineSpacing);
                SDL_RenderDrawLine(renderer, lx1, ledgerY, lx2, ledgerY);
            }
        } else if (note.diatonicStep > 10) { // Above Treble Staff
            for (int step = 12; step <= note.diatonicStep; step += 2) {
                int ledgerY = static_cast<int>(middleCY - step * 0.5f * lineSpacing);
                SDL_RenderDrawLine(renderer, lx1, ledgerY, lx2, ledgerY);
            }
        }

        // Accidental Symbol (# Sharp or b Bemol/Flat) prominently in front of the notehead
        if (note.isAccidental) {
            int accX = noteX1 - 18; // Directly in front of the notehead
            int accY = noteY;

            SDL_SetRenderDrawColor(renderer, 255, 235, 80, 255); // High-contrast gold highlight

            if (note.isFlat) {
                // Large, prominent Bemol / Flat (b) symbol in front of the notehead
                SDL_RenderDrawLine(renderer, accX - 2, accY - 10, accX - 2, accY + 5);
                SDL_RenderDrawLine(renderer, accX - 1, accY - 10, accX - 1, accY + 5);

                SDL_RenderDrawLine(renderer, accX - 2, accY + 5, accX + 4, accY + 2);
                SDL_RenderDrawLine(renderer, accX + 4, accY + 2, accX + 4, accY - 2);
                SDL_RenderDrawLine(renderer, accX + 4, accY - 2, accX - 2, accY);

                SDL_RenderDrawLine(renderer, accX - 2, accY + 4, accX + 3, accY + 2);
                SDL_RenderDrawLine(renderer, accX + 3, accY - 2, accX - 2, accY + 1);
            } else {
                // Large, prominent Sharp (#) symbol in front of the notehead
                SDL_RenderDrawLine(renderer, accX - 3, accY - 8, accX - 3, accY + 8);
                SDL_RenderDrawLine(renderer, accX - 2, accY - 8, accX - 2, accY + 8);

                SDL_RenderDrawLine(renderer, accX + 2, accY - 8, accX + 2, accY + 8);
                SDL_RenderDrawLine(renderer, accX + 3, accY - 8, accX + 3, accY + 8);

                SDL_RenderDrawLine(renderer, accX - 7, accY - 2, accX + 7, accY - 4);
                SDL_RenderDrawLine(renderer, accX - 7, accY - 1, accX + 7, accY - 3);

                SDL_RenderDrawLine(renderer, accX - 7, accY + 4, accX + 7, accY + 2);
                SDL_RenderDrawLine(renderer, accX - 7, accY + 5, accX + 7, accY + 3);
            }
        }

        // Calculate note duration in beat units (where 1.0 = 1 beat, 4.0 = 1 bar)
        float durationSec = note.duration;
        if (durationSec <= 0.01f && (noteX2 > noteX1)) {
            durationSec = (note.endXRatio - note.xRatio) * 20.0f;
        }
        float beatRatio = durationSec / secondsPerBeat;

        // Dynamic Continuous Morphing Classification:
        // Semiquaver (1/16 beat), Dotted Semiquaver (+1/32), Quaver (1/8 beat), Dotted Quaver (+1/16),
        // Crotchet (1 beat), Dotted Crotchet (+1/2 beat), Minim (2 beats), Dotted Minim (+1 beat), Semibreve (4 beats)
        enum class SymbolType {
            Semiquaver, DottedSemiquaver,
            Quaver, DottedQuaver,
            Crotchet, DottedCrotchet,
            Minim, DottedMinim,
            Semibreve
        };

        SymbolType symType = SymbolType::Semiquaver;
        if (beatRatio >= 3.5f) {
            symType = SymbolType::Semibreve;       // Whole Note = 4 beats (or full bar)
        } else if (beatRatio >= 2.5f) {
            symType = SymbolType::DottedMinim;     // Dotted Half Note = 3 beats
        } else if (beatRatio >= 1.75f) {
            symType = SymbolType::Minim;           // Half Note = 2 beats
        } else if (beatRatio >= 1.25f) {
            symType = SymbolType::DottedCrotchet;  // Dotted Quarter Note = 1.5 beats
        } else if (beatRatio >= 0.75f) {
            symType = SymbolType::Crotchet;         // Quarter Note = 1 beat
        } else if (beatRatio >= 0.55f) {
            symType = SymbolType::DottedQuaver;     // Dotted Eighth Note = 0.75 beat
        } else if (beatRatio >= 0.35f) {
            symType = SymbolType::Quaver;           // Eighth Note = 0.5 beat
        } else if (beatRatio >= 0.1875f) {
            symType = SymbolType::DottedSemiquaver; // Dotted Sixteenth Note = 0.375 beat
        } else {
            symType = SymbolType::Semiquaver;       // Sixteenth Note = 0.25 beat
        }

        bool isFilled = (symType != SymbolType::Minim && symType != SymbolType::DottedMinim && symType != SymbolType::Semibreve);
        bool hasStem = (symType != SymbolType::Semibreve);
        bool hasDot = (symType == SymbolType::DottedSemiquaver || symType == SymbolType::DottedQuaver ||
                       symType == SymbolType::DottedCrotchet || symType == SymbolType::DottedMinim);

        int flagCount = 0;
        if (symType == SymbolType::Semiquaver || symType == SymbolType::DottedSemiquaver) {
            flagCount = 2;
        } else if (symType == SymbolType::Quaver || symType == SymbolType::DottedQuaver) {
            flagCount = 1;
        }

        // Stem Direction (downwards for high notes, upwards for low notes)
        bool isStemDown = (note.diatonicStep >= 6) || (note.diatonicStep <= -2 && note.diatonicStep >= -6);

        // Render Primary Note Head
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        drawNoteHead(renderer, noteX1, noteY, isFilled);

        // Render Augmentation Dot if applicable
        if (hasDot) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            drawAugmentationDot(renderer, noteX1, noteY, note.diatonicStep);
        }

        // Render Stem & Flags
        if (hasStem) {
            int stemHeight = static_cast<int>(3.2f * lineSpacing);
            int stemX = isStemDown ? (noteX1 - 5) : (noteX1 + 5);
            int stemEndY = isStemDown ? (noteY + stemHeight) : (noteY - stemHeight);

            SDL_RenderDrawLine(renderer, stemX, noteY, stemX, stemEndY);

            for (int f = 0; f < flagCount; ++f) {
                int flagOffset = f * 5;
                if (isStemDown) {
                    int fy = stemEndY - flagOffset;
                    SDL_RenderDrawLine(renderer, stemX, fy, stemX + 7, fy - 5);
                    SDL_RenderDrawLine(renderer, stemX + 7, fy - 5, stemX + 5, fy - 2);
                } else {
                    int fy = stemEndY + flagOffset;
                    SDL_RenderDrawLine(renderer, stemX, fy, stemX + 7, fy + 5);
                    SDL_RenderDrawLine(renderer, stemX + 7, fy + 5, stemX + 5, fy + 2);
                }
            }
        }

        // Measure-Crossing Tie Arc Logic
        // Check if the tone extends past the 4-beat boundary of the current measure bar
        int startBar = static_cast<int>(note.timeStamp / secondsPerBar);
        float endTime = note.timeStamp + durationSec;
        int endBar = static_cast<int>(endTime / secondsPerBar);

        if (endBar > startBar) {
            int lastTieX = noteX1;
            for (int b = startBar + 1; b <= endBar && b < totalBars; ++b) {
                float barStartTime = b * secondsPerBar;
                int tieX = x + marginLeft + static_cast<int>((barStartTime / 20.0f) * staffWidth) + 12;

                // Render secondary notehead at the start of the next measure bar
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                drawNoteHead(renderer, tieX, noteY, isFilled);

                // Render curved Tie Arc connecting the noteheads across the measure bar line
                SDL_SetRenderDrawColor(renderer, 100, 220, 255, 255);
                drawTieArc(renderer, lastTieX + 6, noteY, tieX - 6, noteY, isStemDown);

                lastTieX = tieX;
            }
        } else if (noteX2 > noteX1 + 4) {
            // Light duration guide line if sustained within the same measure
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, 140);
            SDL_RenderDrawLine(renderer, noteX1 + 6, noteY, noteX2, noteY);
        }
    }

    // 20-Second Sweeping Cursor Line
    int cursorX = x + marginLeft + static_cast<int>((currentTime_ / 20.0f) * staffWidth);
    SDL_SetRenderDrawColor(renderer, 255, 180, 40, 255);
    int topCursorY = static_cast<int>(middleCY - 11.0f * 0.5f * lineSpacing);
    int bottomCursorY = static_cast<int>(middleCY - (-11.0f) * 0.5f * lineSpacing);
    SDL_RenderDrawLine(renderer, cursorX, topCursorY, cursorX, bottomCursorY);
    SDL_RenderDrawLine(renderer, cursorX - 1, topCursorY, cursorX - 1, bottomCursorY);
}
