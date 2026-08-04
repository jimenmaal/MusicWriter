# MusicWriter (C++ only)

MusicWriter is a native C++ Qt application that captures audio from the default input device and displays a real-time frequency histogram for the band 250 Hz – 6000 Hz.

Repository layout
- `cpp/` — C++ Qt application (primary implementation)
  - `CMakeLists.txt` — build rules (detects optional FFTW)
  - `src/` — source files: `MainWindow`, `AudioProcessor`, `SpectrumWidget`, `main.cpp`

Build requirements
- CMake 3.14+
- Qt5 (Widgets, Multimedia) development packages
- A C++17 compiler (MSVC, MinGW, or clang)
- Optional: FFTW3 for faster FFT (CMake will detect it and enable `USE_FFTW`)

Quick build (Windows PowerShell example):

```powershell
cd cpp
mkdir build; cd build
cmake -G "Ninja" ..    # or use "Visual Studio 17 2022" generator
cmake --build . --config Release
```

Run the produced executable `MusicWriterCpp` (from build output).

GitHub

This repo is configured to push to your remote at https://github.com/jimenmaal/MusicWriter.
