#ifndef AUDIOPROCESSOR_H
#define AUDIOPROCESSOR_H

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "WasapiLoopback.h"
#include <portaudio.h>
#if defined(_WIN32)
#include <pa_win_wasapi.h>
#endif

class AudioProcessor {
public:
    enum class Source {
        Speakers,
        Microphone
    };

    enum class DeviceType {
        LoopbackOutput,
        Microphone,
        VirtualKeyboard
    };

    struct DeviceInfo {
        int deviceIndex = paNoDevice;
        std::string name;
        DeviceType type = DeviceType::Microphone;
        bool isDefault = false;
    };

    static constexpr int kSampleRate = 44100;
    static constexpr int kFrameSize = 1024;
    static constexpr int kDesiredFrequencyResolution = 1;
    static constexpr int kMinFrequency = 15;
    static constexpr int kMaxFrequency = 5000;
    static constexpr int kBinCount = (kMaxFrequency - kMinFrequency) / kDesiredFrequencyResolution + 1;
    using SpectrumArray = std::array<float, kBinCount>;
    using WaveformArray = std::array<float, kFrameSize>;

    AudioProcessor();
    ~AudioProcessor();

    bool start(Source source);
    void stop();
    bool hasDevice(Source source) const;
    bool hasInputDevice() const;
    std::vector<DeviceInfo> availableSources() const;
    bool start(int deviceIndex);
    const std::string& selectedDeviceName() const;
    void pollSpectrum(SpectrumArray& outSpectrum);
    void pollWaveform(WaveformArray& outWaveform);

private:
    static int audioCallback(const void* input, void* output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData);
    void processInput(const float* input, unsigned long frameCount);
    void onWasapiAudio(const float* samples, unsigned long count, int channels, double sampleRate);
    void computeSpectrum(const std::vector<float>& chunk);
    void workerLoop();
    bool findWasapiLoopbackDevice(int& deviceIndex) const;
    int findMatchingWasapiLoopbackDevice(int renderDeviceIndex) const;

    PaStream* stream_ = nullptr;
    WasapiLoopback wasapiLoopback_;
    bool isWasapiLoopback_ = false;
    std::vector<float> currentChunk_;
    int sampleFrameSize_ = 0;
    int fftSize_ = 65536;
    double sampleRate_ = kSampleRate;
    int streamChannels_ = 1;
    std::deque<std::vector<float>> pendingChunks_;
    std::mutex chunkMutex_;
    std::condition_variable chunkCv_;
    std::thread workerThread_;
    std::atomic<bool> workerRunning_{false};
    std::atomic<bool> newData_{false};
    std::mutex spectrumMutex_;
    std::mutex waveformMutex_;
    SpectrumArray spectrum_;
    SpectrumArray workingSpectrum_;
    WaveformArray waveform_;
    std::string selectedDeviceName_;
    int selectedDeviceIndex_ = paNoDevice;
};

#endif // AUDIOPROCESSOR_H
