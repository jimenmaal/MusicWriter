# MusicWriter (C++ Qt)

Build requirements
- CMake 3.14+
- Qt5 (Widgets, Multimedia) development packages
- A C++17 compiler (MSVC, MinGW, or clang)

Quick build (Windows PowerShell example):

```powershell
cd cpp
mkdir build; cd build
cmake -G "Ninja" ..    # or use "Visual Studio 17 2022" generator
cmake --build . --config Release
.
```

Run the produced executable `MusicWriterCpp` (from build output).
