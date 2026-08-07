cmake_minimum_required(VERSION 3.14)
project(MusicWriterCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(portaudio REQUIRED)

add_executable(MusicWriterCpp
    src/main.cpp
    src/AudioProcessor.cpp
    src/MainWindow.cpp
    src/SpectrumWidget.cpp
    src/WaveformWidget.cpp
    src/NoteDetector.cpp
    src/NoteDetectionWidget.cpp
    src/PentagramWidget.cpp
    src/MidiPlayer.cpp
    src/PianoWidget.cpp
    src/WasapiLoopback.cpp
    src/Metronome.cpp
)

if(TARGET SDL2::SDL2)
    target_link_libraries(MusicWriterCpp PRIVATE SDL2::SDL2 portaudio winmm)
else()
    target_include_directories(MusicWriterCpp PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(MusicWriterCpp PRIVATE ${SDL2_LIBRARIES} portaudio winmm)
endif()
