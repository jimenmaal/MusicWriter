#include "MainWindow.h"
#include <SDL.h>
#include <cstring>
#include <vector>

static const uint8_t kFont5x7[][5] = {
    // A-Z
    {0x7C,0x12,0x12,0x12,0x7C}, // A
    {0x7E,0x52,0x52,0x52,0x2C}, // B
    {0x3C,0x42,0x42,0x42,0x24}, // C
    {0x7E,0x42,0x42,0x42,0x3C}, // D
    {0x7E,0x4A,0x4A,0x4A,0x42}, // E
    {0x7E,0x0A,0x0A,0x0A,0x02}, // F
    {0x3C,0x42,0x4A,0x4A,0x3A}, // G
    {0x42,0x42,0x7E,0x42,0x42}, // H
    {0x00,0x00,0x7E,0x00,0x00}, // I
    {0x20,0x40,0x42,0x3E,0x02}, // J
    {0x42,0x44,0x78,0x44,0x42}, // K
    {0x7E,0x40,0x40,0x40,0x40}, // L
    {0x7E,0x04,0x18,0x04,0x7E}, // M
    {0x7E,0x04,0x08,0x10,0x7E}, // N
    {0x3C,0x42,0x42,0x42,0x3C}, // O
    {0x7E,0x12,0x12,0x12,0x0C}, // P
    {0x3C,0x42,0x52,0x22,0x5C}, // Q
    {0x7E,0x12,0x12,0x32,0x4C}, // R
    {0x4C,0x52,0x52,0x52,0x22}, // S
    {0x02,0x02,0x7E,0x02,0x02}, // T
    {0x7E,0x40,0x40,0x40,0x7E}, // U
    {0x3E,0x40,0x40,0x40,0x3E}, // V
    {0x7E,0x20,0x18,0x20,0x7E}, // W
    {0x66,0x18,0x18,0x18,0x66}, // X
    {0x06,0x08,0x70,0x08,0x06}, // Y
    {0x62,0x52,0x4A,0x46,0x42}, // Z
    // 0-9
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x62,0x51,0x49,0x49,0x46}, // 2
    {0x22,0x41,0x49,0x49,0x36}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x2F,0x49,0x49,0x49,0x31}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}  // 9
};

static int getFontIndex(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<int>(c - 'A');
    }
    if (c >= '0' && c <= '9') {
        return 26 + static_cast<int>(c - '0');
    }
    return -1;
}

void MainWindow::drawChar(char c, int x, int y, int scale) {
    int idx = getFontIndex(c);
    if (idx < 0) return;
    const uint8_t* pattern = kFont5x7[idx];
    for (int col = 0; col < 5; ++col) {
        uint8_t column = pattern[col];
        for (int row = 0; row < 7; ++row) {
            if (column & (1 << row)) {
                SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer_, &pixel);
            }
        }
    }
}

void MainWindow::renderText(const char* text, int x, int y, int scale) {
    int offset = 0;
    for (size_t i = 0; i < std::strlen(text); ++i) {
        char c = text[i];
        if (c == ' ') {
            offset += 6 * scale;
            continue;
        }
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        drawChar(c, x + offset, y, scale);
        offset += 6 * scale;
    }
}

static const char* getDeviceTypeLabel(AudioProcessor::DeviceType type) {
    switch (type) {
        case AudioProcessor::DeviceType::LoopbackOutput: return "OUTPUT";
        case AudioProcessor::DeviceType::Microphone: return "MIC";
        default: return "DEVICE";
    }
}

void MainWindow::updateAudioSources() {
    sourceDevices_ = audio_.availableSources();
    selectedSourceIndex_ = sourceDevices_.empty() ? -1 : 0;
    audioAvailable_ = selectedSourceIndex_ >= 0;
}

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
}

bool MainWindow::init() {
    window_ = SDL_CreateWindow("MusicWriter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) return false;

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) return false;

    updateAudioSources();
    if (!audioAvailable_) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Audio Source", "No active audio sources detected. Connect a microphone or enable speaker loopback and restart.", window_);
    }

    running_ = true;
    recording_ = false;
    return true;
}

bool MainWindow::isVirtualKeyboardActive() const {
    if (selectedSourceIndex_ >= 0 && selectedSourceIndex_ < static_cast<int>(sourceDevices_.size())) {
        return sourceDevices_[selectedSourceIndex_].type == AudioProcessor::DeviceType::VirtualKeyboard;
    }
    return false;
}

void MainWindow::run() {
    const Uint64 frameDuration = 1000 / 30;
    lastFrameTicks_ = SDL_GetTicks64();
    while (running_) {
        Uint64 frameStart = SDL_GetTicks64();
        float dt = static_cast<float>(frameStart - lastFrameTicks_) / 1000.0f;
        if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 30.0f;
        lastFrameTicks_ = frameStart;

        handleEvents();

        if (midiPlayer_.isPlaying()) {
            midiPlayer_.update(dt);
            pentagramWidget_.setPlaybackCursor(midiPlayer_.playbackTime());
        } else if (isVirtualKeyboardActive()) {
            pentagramWidget_.update(virtualNotes_, dt, recording_);
        } else if (recording_ && audioAvailable_) {
            audio_.pollSpectrum(spectrum_.data());
            noteDetector_.detectNotes(spectrum_.data(), detectedNotes_);
            noteWidget_.setNotes(detectedNotes_);
            pentagramWidget_.update(detectedNotes_, dt, true);
        } else {
            detectedNotes_.clear();
            noteWidget_.setNotes(detectedNotes_);
            pentagramWidget_.update(detectedNotes_, dt, false);
        }

        metronome_.update(dt);

        render();
        Uint64 frameTime = SDL_GetTicks64() - frameStart;
        if (frameTime < frameDuration) {
            SDL_Delay(static_cast<Uint32>(frameDuration - frameTime));
        }
    }
    if (recording_ && audioAvailable_ && !isVirtualKeyboardActive()) {
        audio_.stop();
    }
    midiPlayer_.stopPlayback();
}

void MainWindow::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
        }
        else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_SPACE) {
                // Space Bar toggles recording ON / OFF
                if (!recording_ && audioAvailable_) {
                    midiPlayer_.stopPlayback();
                    metronome_.reset();
                    if (isVirtualKeyboardActive()) {
                        recording_ = true;
                    } else if (selectedSourceIndex_ >= 0 && selectedSourceIndex_ < static_cast<int>(sourceDevices_.size())) {
                        recording_ = audio_.start(sourceDevices_[selectedSourceIndex_].deviceIndex);
                        if (!recording_) {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Audio Input Error", "Unable to open the selected audio source. Please select another source.", window_);
                        }
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Audio Input Error", "No audio source selected. Please select an active source.", window_);
                    }
                } else if (recording_) {
                    if (!isVirtualKeyboardActive()) {
                        audio_.stop();
                    }
                    recording_ = false;
                }
            } else if (isVirtualKeyboardActive() && !event.key.repeat) {
                int midiNote = PianoWidget::getMidiFromKey(event.key.keysym.sym, event.key.keysym.mod);
                if (midiNote >= 43 && midiNote <= 90) {
                    pianoWidget_.setKeyPressed(midiNote, true);

                    // Trigger live MIDI audio sound feedback immediately
                    midiPlayer_.sendNoteOn(midiNote, 100);

                    static const char* kNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                    int noteIdx = ((midiNote % 12) + 12) % 12;
                    int octave = (midiNote / 12) - 1;
                    float freq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);

                    DetectedNote dNote;
                    dNote.name = kNoteNames[noteIdx];
                    dNote.octave = octave;
                    dNote.frequency = freq;
                    dNote.cents = 0.0f;
                    dNote.magnitude = 1.0f;
                    dNote.midiNote = midiNote;

                    bool exists = false;
                    for (const auto& vn : virtualNotes_) {
                        if (vn.midiNote == midiNote) { exists = true; break; }
                    }
                    if (!exists) {
                        virtualNotes_.push_back(dNote);
                    }
                }
            }
        }
        else if (event.type == SDL_KEYUP) {
            if (isVirtualKeyboardActive()) {
                int midiNote = PianoWidget::getMidiFromKey(event.key.keysym.sym, event.key.keysym.mod);
                if (midiNote < 0) {
                    midiNote = PianoWidget::getMidiFromKey(event.key.keysym.sym, 0);
                }
                if (midiNote < 0) {
                    midiNote = PianoWidget::getMidiFromKey(event.key.keysym.sym, KMOD_SHIFT);
                }

                if (midiNote >= 43 && midiNote <= 90) {
                    pianoWidget_.setKeyPressed(midiNote, false);

                    // Silence MIDI note immediately on key release
                    midiPlayer_.sendNoteOff(midiNote);

                    virtualNotes_.erase(
                        std::remove_if(virtualNotes_.begin(), virtualNotes_.end(), [midiNote](const DetectedNote& n) {
                            return n.midiNote == midiNote;
                        }),
                        virtualNotes_.end()
                    );
                }
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            int mx = event.button.x;
            int my = event.button.y;

            // Start / Stop Recording Button
            if (mx >= buttonRect_.x && mx <= buttonRect_.x + buttonRect_.w && my >= buttonRect_.y && my <= buttonRect_.y + buttonRect_.h) {
                if (!recording_ && audioAvailable_) {
                    midiPlayer_.stopPlayback();
                    metronome_.reset();
                    if (isVirtualKeyboardActive()) {
                        recording_ = true;
                    } else if (selectedSourceIndex_ >= 0 && selectedSourceIndex_ < static_cast<int>(sourceDevices_.size())) {
                        recording_ = audio_.start(sourceDevices_[selectedSourceIndex_].deviceIndex);
                        if (!recording_) {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Audio Input Error", "Unable to open the selected audio source. Please select another source.", window_);
                        }
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Audio Input Error", "No audio source selected. Please select an active source.", window_);
                    }
                } else if (recording_) {
                    if (!isVirtualKeyboardActive()) {
                        audio_.stop();
                    }
                    recording_ = false;
                }
            }
            // Play / Stop MIDI Playback Button
            else if (mx >= playButtonRect_.x && mx <= playButtonRect_.x + playButtonRect_.w && my >= playButtonRect_.y && my <= playButtonRect_.y + playButtonRect_.h) {
                if (midiPlayer_.isPlaying()) {
                    midiPlayer_.stopPlayback();
                } else {
                    if (recording_) {
                        if (!isVirtualKeyboardActive()) audio_.stop();
                        recording_ = false;
                    }
                    const auto& notes = pentagramWidget_.recordedNotes();
                    if (notes.empty()) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "MIDI Playback", "No recorded notes found on the pentagram. Record some notes first!", window_);
                    } else {
                        metronome_.reset();
                        midiPlayer_.startPlayback(notes, 20.0f);
                    }
                }
            }
            // Metronome Toggle Button
            else if (mx >= metronomeButtonRect_.x && mx <= metronomeButtonRect_.x + metronomeButtonRect_.w && my >= metronomeButtonRect_.y && my <= metronomeButtonRect_.y + metronomeButtonRect_.h) {
                metronome_.setEnabled(!metronome_.isEnabled());
            }
            // BPM Minus Button
            else if (mx >= bpmMinusRect_.x && mx <= bpmMinusRect_.x + bpmMinusRect_.w && my >= bpmMinusRect_.y && my <= bpmMinusRect_.y + bpmMinusRect_.h) {
                int newBpm = std::max(40, metronome_.bpm() - 5);
                metronome_.setBpm(newBpm);
                pentagramWidget_.setBpm(newBpm);
            }
            // BPM Plus Button
            else if (mx >= bpmPlusRect_.x && mx <= bpmPlusRect_.x + bpmPlusRect_.w && my >= bpmPlusRect_.y && my <= bpmPlusRect_.y + bpmPlusRect_.h) {
                int newBpm = std::min(240, metronome_.bpm() + 5);
                metronome_.setBpm(newBpm);
                pentagramWidget_.setBpm(newBpm);
            }
            // Time Signature Button
            else if (mx >= timeSigButtonRect_.x && mx <= timeSigButtonRect_.x + timeSigButtonRect_.w && my >= timeSigButtonRect_.y && my <= timeSigButtonRect_.y + timeSigButtonRect_.h) {
                TimeSignature nextTs = TimeSignature::TS_4_4;
                if (metronome_.timeSignature() == TimeSignature::TS_4_4) nextTs = TimeSignature::TS_3_4;
                else if (metronome_.timeSignature() == TimeSignature::TS_3_4) nextTs = TimeSignature::TS_2_4;
                else if (metronome_.timeSignature() == TimeSignature::TS_2_4) nextTs = TimeSignature::TS_6_8;
                else if (metronome_.timeSignature() == TimeSignature::TS_6_8) nextTs = TimeSignature::TS_4_4;
                metronome_.setTimeSignature(nextTs);
                pentagramWidget_.setTimeSignature(nextTs);
            }
            // Pause Between Bars Toggle Button
            else if (mx >= pauseBarsButtonRect_.x && mx <= pauseBarsButtonRect_.x + pauseBarsButtonRect_.w && my >= pauseBarsButtonRect_.y && my <= pauseBarsButtonRect_.y + pauseBarsButtonRect_.h) {
                pentagramWidget_.setPauseBetweenBars(!pentagramWidget_.pauseBetweenBars());
            }
            // Clear Score / Reset Button
            else if (mx >= resetButtonRect_.x && mx <= resetButtonRect_.x + resetButtonRect_.w && my >= resetButtonRect_.y && my <= resetButtonRect_.y + resetButtonRect_.h) {
                if (midiPlayer_.isPlaying()) {
                    midiPlayer_.stopPlayback();
                }
                if (recording_) {
                    if (!isVirtualKeyboardActive()) audio_.stop();
                    recording_ = false;
                }
                pentagramWidget_.reset();
                noteDetector_.reset();
                pianoWidget_.clearAllKeys();
                virtualNotes_.clear();
            }
            // Source Panel Selection
            else if (mx >= sourcePanelRect_.x && mx <= sourcePanelRect_.x + sourcePanelRect_.w && my >= sourcePanelRect_.y && my <= sourcePanelRect_.y + sourcePanelRect_.h) {
                if (!recording_) {
                    for (int i = 0; i < static_cast<int>(sourceDevices_.size()) && i < 16; ++i) {
                        const SDL_Rect& itemRect = sourceItemRects_[i];
                        if (mx >= itemRect.x && mx <= itemRect.x + itemRect.w && my >= itemRect.y && my <= itemRect.y + itemRect.h) {
                            selectedSourceIndex_ = i;
                            audioAvailable_ = true;
                            pianoWidget_.clearAllKeys();
                            virtualNotes_.clear();
                            break;
                        }
                    }
                } else {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Audio Source", "Stop recording before switching audio sources.", window_);
                }
            }
        }
    }
}

void MainWindow::renderButton() {
    if (recording_) {
        SDL_SetRenderDrawColor(renderer_, 180, 40, 40, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 40, 140, 60, 255);
    }
    SDL_RenderFillRect(renderer_, &buttonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &buttonRect_);

    const char* label = recording_ ? "STOP REC" : "START REC";
    int labelX = buttonRect_.x + 10;
    int labelY = buttonRect_.y + 12;
    renderText(label, labelX, labelY, 3);
}

void MainWindow::renderPlayButton() {
    if (midiPlayer_.isPlaying()) {
        SDL_SetRenderDrawColor(renderer_, 200, 100, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 40, 100, 180, 255);
    }
    SDL_RenderFillRect(renderer_, &playButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &playButtonRect_);

    const char* label = midiPlayer_.isPlaying() ? "STOP PLAY" : "PLAY MIDI";
    int labelX = playButtonRect_.x + 10;
    int labelY = playButtonRect_.y + 12;
    renderText(label, labelX, labelY, 3);
}

void MainWindow::renderSourcePanel() {
    SDL_SetRenderDrawColor(renderer_, 48, 48, 48, 255);
    SDL_RenderFillRect(renderer_, &sourcePanelRect_);
    SDL_SetRenderDrawColor(renderer_, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer_, &sourcePanelRect_);

    renderText("AUDIO SOURCES (WASAPI)", sourcePanelRect_.x + 12, sourcePanelRect_.y + 10, 2);
    int itemHeight = 38;
    int padding = 8;
    int top = sourcePanelRect_.y + 36;
    int count = static_cast<int>(sourceDevices_.size());
    if (count > 16) count = 16;
    for (int i = 0; i < count; ++i) {
        SDL_Rect itemRect = { sourcePanelRect_.x + padding, top + i * (itemHeight + 4), sourcePanelRect_.w - padding * 2, itemHeight };
        sourceItemRects_[i] = itemRect;

        if (i == selectedSourceIndex_) {
            SDL_SetRenderDrawColor(renderer_, 45, 90, 160, 255);
            SDL_RenderFillRect(renderer_, &itemRect);
            SDL_SetRenderDrawColor(renderer_, 0, 210, 240, 255);
        } else {
            SDL_SetRenderDrawColor(renderer_, 28, 28, 28, 255);
            SDL_RenderFillRect(renderer_, &itemRect);
            SDL_SetRenderDrawColor(renderer_, 120, 120, 120, 255);
        }
        SDL_RenderDrawRect(renderer_, &itemRect);

        const auto& info = sourceDevices_[i];

        // Row 1: Device Name (Scale 1)
        SDL_SetRenderDrawColor(renderer_, 240, 240, 240, 255);
        renderText(info.name.c_str(), itemRect.x + 6, itemRect.y + 5, 1);

        // Row 2: Category & Default Tag (Scale 1)
        std::string tag = std::string("[") + getDeviceTypeLabel(info.type) + "]";
        if (info.isDefault) {
            tag += " [DEFAULT]";
        }
        if (i == selectedSourceIndex_) {
            SDL_SetRenderDrawColor(renderer_, 255, 200, 0, 255);
        } else {
            SDL_SetRenderDrawColor(renderer_, 160, 160, 160, 255);
        }
        renderText(tag.c_str(), itemRect.x + 6, itemRect.y + 20, 1);
    }
}

void MainWindow::renderStatus() {
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    if (midiPlayer_.isPlaying()) {
        renderText("PLAYING MIDI BACK", 200, 30, 3);
    } else if (!audioAvailable_) {
        renderText("NO AUDIO SOURCE", 200, 30, 3);
    } else if (pentagramWidget_.isBarPaused()) {
        SDL_SetRenderDrawColor(renderer_, 255, 180, 40, 255);
        renderText("BAR END PAUSED - PLAY NEXT TONE TO RESUME", 200, 30, 2);
    } else if (recording_) {
        if (isVirtualKeyboardActive()) {
            renderText("RECORDING (PRESS SPACE TO STOP)", 200, 30, 3);
        } else {
            renderText("RECORDING", 200, 30, 3);
        }
    } else {
        if (isVirtualKeyboardActive()) {
            renderText("PRESS SPACE TO START RECORDING", 200, 30, 3);
        } else {
            renderText("READY - START REC OR PLAY MIDI", 200, 30, 3);
        }
    }

    if (selectedSourceIndex_ >= 0 && selectedSourceIndex_ < static_cast<int>(sourceDevices_.size())) {
        const std::string& deviceName = sourceDevices_[selectedSourceIndex_].name;
        int scale = (deviceName.length() > 30) ? 1 : 2;
        renderText(deviceName.c_str(), 200, 60, scale);
    } else {
        renderText("NO DEVICE SELECTED", 200, 60, 2);
    }
}

void MainWindow::render() {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    int padding = 12;
    int leftColumnWidth = 280;
    int rightColumnX = padding + leftColumnWidth + padding;
    int contentWidth = width - rightColumnX - padding;
    if (contentWidth < 300) contentWidth = 300;

    buttonRect_ = {padding, padding, leftColumnWidth, 40};
    playButtonRect_ = {padding, buttonRect_.y + buttonRect_.h + 6, leftColumnWidth, 40};
    metronomeButtonRect_ = {padding, playButtonRect_.y + playButtonRect_.h + 6, leftColumnWidth, 34};
    bpmMinusRect_ = {padding, metronomeButtonRect_.y + metronomeButtonRect_.h + 4, 55, 28};
    bpmPlusRect_ = {padding + leftColumnWidth - 55, metronomeButtonRect_.y + metronomeButtonRect_.h + 4, 55, 28};
    timeSigButtonRect_ = {padding, bpmMinusRect_.y + bpmMinusRect_.h + 6, leftColumnWidth, 34};
    pauseBarsButtonRect_ = {padding, timeSigButtonRect_.y + timeSigButtonRect_.h + 6, leftColumnWidth, 34};
    resetButtonRect_ = {padding, pauseBarsButtonRect_.y + pauseBarsButtonRect_.h + 6, leftColumnWidth, 34};
    sourcePanelRect_ = {padding, resetButtonRect_.y + resetButtonRect_.h + 6, leftColumnWidth, height - (resetButtonRect_.y + resetButtonRect_.h + 6) - padding};

    renderButton();
    renderPlayButton();

    // Render Metronome Toggle Button
    if (metronome_.isEnabled()) {
        SDL_SetRenderDrawColor(renderer_, 180, 120, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 60, 60, 70, 255);
    }
    SDL_RenderFillRect(renderer_, &metronomeButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &metronomeButtonRect_);
    std::string metroLabel = std::string("METRONOME [") + (metronome_.isEnabled() ? "ON" : "OFF") + "]";
    renderText(metroLabel.c_str(), metronomeButtonRect_.x + 8, metronomeButtonRect_.y + 8, 2);

    // Render BPM Controls
    SDL_SetRenderDrawColor(renderer_, 50, 70, 90, 255);
    SDL_RenderFillRect(renderer_, &bpmMinusRect_);
    SDL_RenderFillRect(renderer_, &bpmPlusRect_);
    SDL_SetRenderDrawColor(renderer_, 200, 220, 240, 255);
    SDL_RenderDrawRect(renderer_, &bpmMinusRect_);
    SDL_RenderDrawRect(renderer_, &bpmPlusRect_);
    renderText("-5", bpmMinusRect_.x + 10, bpmMinusRect_.y + 6, 2);
    renderText("+5", bpmPlusRect_.x + 10, bpmPlusRect_.y + 6, 2);

    std::string bpmText = std::to_string(metronome_.bpm()) + " BPM";
    int bpmX = bpmMinusRect_.x + bpmMinusRect_.w + 14;
    renderText(bpmText.c_str(), bpmX, bpmMinusRect_.y + 6, 2);

    // Render Time Signature Button
    SDL_SetRenderDrawColor(renderer_, 0, 110, 150, 255);
    SDL_RenderFillRect(renderer_, &timeSigButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &timeSigButtonRect_);
    std::string tsLabel = std::string("TIME SIG [") + getTimeSignatureLabel(metronome_.timeSignature()) + "]";
    renderText(tsLabel.c_str(), timeSigButtonRect_.x + 8, timeSigButtonRect_.y + 8, 2);

    // Render Pause Between Bars Toggle Button
    if (pentagramWidget_.pauseBetweenBars()) {
        SDL_SetRenderDrawColor(renderer_, 140, 60, 180, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 50, 50, 60, 255);
    }
    SDL_RenderFillRect(renderer_, &pauseBarsButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &pauseBarsButtonRect_);
    std::string pauseLabel = std::string("PAUSE BARS [") + (pentagramWidget_.pauseBetweenBars() ? "ON" : "OFF") + "]";
    renderText(pauseLabel.c_str(), pauseBarsButtonRect_.x + 8, pauseBarsButtonRect_.y + 8, 2);

    // Render Clear Score / Reset Button
    SDL_SetRenderDrawColor(renderer_, 180, 45, 45, 255); // Crimson red accent
    SDL_RenderFillRect(renderer_, &resetButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &resetButtonRect_);
    renderText("CLEAR SCORE", resetButtonRect_.x + 8, resetButtonRect_.y + 8, 2);

    renderSourcePanel();
    renderStatus();

    auto renderTextLambda = [this](const char* text, int x, int y, int scale) {
        this->renderText(text, x, y, scale);
    };

    if (isVirtualKeyboardActive()) {
        // 2-panel right column layout (Pentagram 50%, Piano Diagram 50%)
        int totalAvailableHeight = height - padding * 3;
        if (totalAvailableHeight < 360) totalAvailableHeight = 360;

        int pentagramHeight = static_cast<int>(totalAvailableHeight * 0.50f);
        int pianoHeight = totalAvailableHeight - pentagramHeight;

        // Panel 1: Grand Staff Pentagram Visualizer (50% height)
        SDL_Rect pentagramArea = { rightColumnX, padding, contentWidth, pentagramHeight };
        SDL_SetRenderDrawColor(renderer_, 14, 20, 30, 255);
        SDL_RenderFillRect(renderer_, &pentagramArea);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &pentagramArea);
        pentagramWidget_.render(renderer_, pentagramArea.x + 4, pentagramArea.y + 4, pentagramArea.w - 8, pentagramArea.h - 8, renderTextLambda);

        // Panel 2: Interactive 2-Octave Piano Keyboard Diagram (50% height)
        SDL_Rect pianoArea = { rightColumnX, pentagramArea.y + pentagramArea.h + padding, contentWidth, pianoHeight };
        SDL_SetRenderDrawColor(renderer_, 18, 24, 34, 255);
        SDL_RenderFillRect(renderer_, &pianoArea);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &pianoArea);
        pianoWidget_.render(renderer_, pianoArea.x + 4, pianoArea.y + 4, pianoArea.w - 8, pianoArea.h - 8, renderTextLambda);
    } else {
        // 3-panel right column layout (Pentagram 50%, Note Detector 25%, Spectrum 25%)
        int totalAvailableHeight = height - padding * 4;
        if (totalAvailableHeight < 360) totalAvailableHeight = 360;

        int pentagramHeight = static_cast<int>(totalAvailableHeight * 0.50f);
        int noteHeight = static_cast<int>(totalAvailableHeight * 0.25f);
        int spectrumHeight = totalAvailableHeight - pentagramHeight - noteHeight;

        // Panel 1: Grand Staff Pentagram Visualizer (50% height)
        SDL_Rect pentagramArea = { rightColumnX, padding, contentWidth, pentagramHeight };
        SDL_SetRenderDrawColor(renderer_, 14, 20, 30, 255);
        SDL_RenderFillRect(renderer_, &pentagramArea);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &pentagramArea);
        pentagramWidget_.render(renderer_, pentagramArea.x + 4, pentagramArea.y + 4, pentagramArea.w - 8, pentagramArea.h - 8, renderTextLambda);

        // Panel 2: Fundamental Note Detection (25% height)
        SDL_Rect noteArea = { rightColumnX, pentagramArea.y + pentagramArea.h + padding, contentWidth, noteHeight };
        SDL_SetRenderDrawColor(renderer_, 18, 24, 34, 255);
        SDL_RenderFillRect(renderer_, &noteArea);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &noteArea);
        noteWidget_.render(renderer_, noteArea.x + 4, noteArea.y + 4, noteArea.w - 8, noteArea.h - 8, renderTextLambda);

        // Panel 3: Frequency Spectrum Histogram (25% height)
        SDL_Rect spectrumArea = { rightColumnX, noteArea.y + noteArea.h + padding, contentWidth, spectrumHeight };
        SDL_SetRenderDrawColor(renderer_, 32, 32, 32, 255);
        SDL_RenderFillRect(renderer_, &spectrumArea);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer_, &spectrumArea);
        spectrum_.render(renderer_, spectrumArea.x + 4, spectrumArea.y + 4, spectrumArea.w - 8, spectrumArea.h - 8);
    }

    SDL_RenderPresent(renderer_);
}
