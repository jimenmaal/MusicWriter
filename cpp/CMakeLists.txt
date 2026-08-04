cmake_minimum_required(VERSION 3.14)
project(MusicWriterCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt5 COMPONENTS Widgets Multimedia REQUIRED)

# Optionally find FFTW3
find_library(FFTW3_LIB fftw3)
if(FFTW3_LIB)
    message(STATUS "Found fftw3: ${FFTW3_LIB}")
else()
    message(STATUS "fftw3 not found; building with internal FFT")
endif()

add_executable(MusicWriterCpp
    src/main.cpp
    src/MainWindow.cpp
    src/AudioProcessor.cpp
    src/SpectrumWidget.cpp
)

target_include_directories(MusicWriterCpp PRIVATE src)
target_link_libraries(MusicWriterCpp PRIVATE Qt5::Widgets Qt5::Multimedia)

if(FFTW3_LIB)
    target_link_libraries(MusicWriterCpp PRIVATE ${FFTW3_LIB})
    target_compile_definitions(MusicWriterCpp PRIVATE USE_FFTW)
endif()
