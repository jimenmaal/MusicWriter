# MusicWriterCpp

MusicWriterCpp is a native desktop audio analysis application written in C++17. It captures live audio from the system, computes frequency-spectrum information in real time, and displays both a frequency histogram and a raw waveform view in an SDL2-based UI.

## Main Functionality

- Detects active audio sources automatically using PortAudio.
- Supports speaker loopback and microphone input devices.
- Displays a selectable source list in the left pane.
- Captures live audio and converts it into waveform and spectrum data.
- Computes an FFT-based frequency histogram for the range 15 Hz to 5000 Hz.
- Renders the spectrum and waveform continuously at an application frame rate.

## Architecture Overview

The project is organized into a small C++ application with four main runtime components:

### 1. `AudioProcessor`

Defined in `cpp/src/AudioProcessor.h` / `cpp/src/AudioProcessor.cpp`.

Responsibilities:
- Initializes PortAudio and enumerates audio devices.
- Detects loopback-capable output devices and microphone devices.
- Exposes available sources through `availableSources()`.
- Opens a PortAudio stream for a selected audio device.
- Receives raw audio frames in `audioCallback()`.
- Converts multi-channel input to mono and buffers 1/30-second chunks.
- Sends ready chunks to a worker thread for spectrum processing.
- Computes a Hann-windowed FFT and maps magnitude bins into a fixed-frequency histogram.
- Provides thread-safe polling methods: `pollSpectrum()` and `pollWaveform()`.

Key design points:
- Uses a worker thread to separate audio capture from FFT computation.
- Preserves UI responsiveness by queuing full 1/30-second audio chunks.
- Normalizes spectrum output so rendered bars fit the histogram panel.

### 2. `MainWindow`

Defined in `cpp/src/MainWindow.h` / `cpp/src/MainWindow.cpp`.

Responsibilities:
- Creates the SDL window and renderer.
- Manages the application event loop and frame timing.
- Fetches available audio sources from `AudioProcessor`.
- Handles user input for starting/stopping capture and selecting a source.
- Renders the left configuration panel and the right output panels.
- Calls `AudioProcessor::pollSpectrum()` and `pollWaveform()` each frame.

Layout:
- Left column: `Start/Stop` button plus the active audio source list.
- Right column: frequency histogram on top, raw waveform panel below.

### 3. `SpectrumWidget`

Defined in `cpp/src/SpectrumWidget.h` / `cpp/src/SpectrumWidget.cpp`.

Responsibilities:
- Stores the latest spectrum frame.
- Renders frequency histogram bars across the available panel width.
- Aggregates many FFT bins into visible screen bars to fit the panel properly.
- Draws a color gradient to improve visual interpretation.

### 4. `WaveformWidget`

Defined in `cpp/src/WaveformWidget.h` / `cpp/src/WaveformWidget.cpp`.

Responsibilities:
- Stores the latest raw waveform frame.
- Renders a polyline waveform centered vertically in the waveform panel.
- Scales samples to fit the panel height.

## Build and Run

### Prerequisites

- C++17 compatible compiler / toolchain
- CMake 3.14 or newer
- SDL2 development package
- PortAudio development package

### Build

From the `cpp` directory:

```sh
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

### Run

From the build directory:

```sh
.\Release\MusicWriterCpp.exe
```

## Project Structure

```
cpp/
  CMakeLists.txt
  src/
    main.cpp
    AudioProcessor.cpp
    AudioProcessor.h
    MainWindow.cpp
    MainWindow.h
    SpectrumWidget.cpp
    SpectrumWidget.h
    WaveformWidget.cpp
    WaveformWidget.h
```

## Key Project Concepts

- **Device detection**: The app scans PortAudio devices and identifies valid audio sources automatically. It aims to include output loopback devices (speakers/headset) and any available microphones.
- **Threaded audio processing**: Audio capture is continuous in the PortAudio callback, while spectrum computation runs in a separate worker thread.
- **Frequency range**: The histogram is computed for 15 Hz to 5000 Hz, with one bin per Hz in the internal representation.
- **Render layout**: The UI keeps controls and configuration on the left, while the frequency and waveform panels occupy the right side.

## Notes

- The application currently uses a custom built-in 5x7 ASCII font renderer for labels.
- If no valid sources are detected, the app informs the user and disables recording until a source becomes available.

## Future Improvements

- Add support for choosing sample rate and channel count explicitly.
- Improve the spectrum UI with frequency axis labels and smoother bar interpolation.
- Add persistence for last-selected audio device.
- Include real-time logging or status/error messages inside the UI.
