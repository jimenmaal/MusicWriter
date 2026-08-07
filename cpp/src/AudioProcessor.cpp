#include "AudioProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#define NOMINMAX
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    void debugLog(const char* message) {
        char buffer[MAX_PATH];
        if (GetModuleFileNameA(nullptr, buffer, MAX_PATH) == 0) {
            return;
        }
        std::string path(buffer);
        auto pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            path = path.substr(0, pos + 1);
        } else {
            path = "";
        }
        path += "MusicWriter.log";
        std::ofstream out(path, std::ios::app);
        if (out.is_open()) {
            out << message << "\n";
        }
    }
}

namespace {
    void logPortAudioDevices() {
        int deviceCount = Pa_GetDeviceCount();
        std::string countLog = std::string("AudioProcessor: PortAudio device count = ") + std::to_string(deviceCount);
        debugLog(countLog.c_str());
        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info) continue;
            const PaHostApiInfo* hostApi = Pa_GetHostApiInfo(info->hostApi);
            std::string hostName = hostApi && hostApi->name ? hostApi->name : "Unknown";
            std::string deviceLog = std::string("AudioProcessor: device[") + std::to_string(i) + "] name=\"" +
                (info->name ? info->name : "Unknown") + "\" host=\"" + hostName +
                "\" input=" + std::to_string(info->maxInputChannels) +
                " output=" + std::to_string(info->maxOutputChannels) +
                " defaultSampleRate=" + std::to_string(info->defaultSampleRate);
            debugLog(deviceLog.c_str());
#if defined(PA_WASAPI)
            std::string loopbackLog = std::string("AudioProcessor: device[") + std::to_string(i) + "] WASAPI loopback=" +
                (PaWasapi_IsLoopbackDevice(i) ? "yes" : "no");
            debugLog(loopbackLog.c_str());
#endif
        }
    }

    void applyHannWindow(float* data, int size) {
        for (int i = 0; i < size; ++i) {
            data[i] *= 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
        }
    }

    void computeMagnitude(const float* re, const float* im, float* output, int size) {
        for (int i = 0; i < size; ++i) {
            output[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]);
        }
    }

    void fft(float* real, float* imag, int n) {
        int j = 0;
        for (int i = 1; i < n - 1; ++i) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(real[i], real[j]);
                std::swap(imag[i], imag[j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1) {
            float angle = -2.0f * M_PI / len;
            float wlenR = std::cos(angle);
            float wlenI = std::sin(angle);
            for (int i = 0; i < n; i += len) {
                float ur = 1.0f;
                float ui = 0.0f;
                for (int j = 0; j < len / 2; ++j) {
                    int evenIndex = i + j;
                    int oddIndex = i + j + len / 2;
                    float vr = real[oddIndex] * ur - imag[oddIndex] * ui;
                    float vi = real[oddIndex] * ui + imag[oddIndex] * ur;
                    real[oddIndex] = real[evenIndex] - vr;
                    imag[oddIndex] = imag[evenIndex] - vi;
                    real[evenIndex] += vr;
                    imag[evenIndex] += vi;
                    float nextUr = ur * wlenR - ui * wlenI;
                    ui = ur * wlenI + ui * wlenR;
                    ur = nextUr;
                }
            }
        }
    }
}

AudioProcessor::AudioProcessor() {
    sampleFrameSize_ = static_cast<int>(kSampleRate / 30.0);
    fftSize_ = 1;
    while (fftSize_ < sampleFrameSize_) {
        fftSize_ <<= 1;
    }
    fftSize_ <<= 1;
    currentChunk_.reserve(sampleFrameSize_ * 2);
    spectrum_.fill(0.0f);
    workingSpectrum_.fill(0.0f);
    waveform_.fill(0.0f);
    Pa_Initialize();
    logPortAudioDevices();
}

AudioProcessor::~AudioProcessor() {
    workerRunning_ = false;
    chunkCv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    stop();
    Pa_Terminate();
}

bool AudioProcessor::start(Source source) {
    int deviceCount = Pa_GetDeviceCount();
    int deviceIndex = paNoDevice;

    if (source == Source::Microphone) {
        int defaultDevice = Pa_GetDefaultInputDevice();
        if (defaultDevice != paNoDevice) {
            deviceIndex = defaultDevice;
        } else {
            for (int i = 0; i < deviceCount; ++i) {
                const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
                if (deviceInfo && deviceInfo->maxInputChannels > 0) {
                    deviceIndex = i;
                    break;
                }
            }
        }
    } else {
        if (!findWasapiLoopbackDevice(deviceIndex)) {
            debugLog("AudioProcessor: WASAPI loopback device not found, falling back to default output device");
            int defaultOutput = Pa_GetDefaultOutputDevice();
            if (defaultOutput != paNoDevice) {
                const PaDeviceInfo* outputInfo = Pa_GetDeviceInfo(defaultOutput);
                if (outputInfo && outputInfo->maxInputChannels > 0) {
                    deviceIndex = defaultOutput;
                    debugLog("AudioProcessor: default output device has input channels and is selected");
                } else if (outputInfo) {
                    std::string defaultLog = std::string("AudioProcessor: default output device invalid: ") + (outputInfo->name ? outputInfo->name : "Unknown");
                    debugLog(defaultLog.c_str());
                }
            }
        }

        if (deviceIndex == paNoDevice) {
            for (int i = 0; i < deviceCount; ++i) {
                const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
                if (deviceInfo && deviceInfo->maxInputChannels > 0 && deviceInfo->maxOutputChannels > 0) {
                    deviceIndex = i;
                    debugLog("AudioProcessor: found device with both input and output channels");
                    break;
                }
            }
        }

        if (deviceIndex == paNoDevice) {
            for (int i = 0; i < deviceCount; ++i) {
                const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
                if (deviceInfo && deviceInfo->maxInputChannels > 0) {
                    deviceIndex = i;
                    debugLog("AudioProcessor: falling back to any input-capable device");
                    break;
                }
            }
        }
    }

    if (deviceIndex == paNoDevice) {
        selectedDeviceName_.clear();
        debugLog("AudioProcessor: no device selected");
        return false;
    }

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo || deviceInfo->maxInputChannels == 0) {
        selectedDeviceName_.clear();
        debugLog("AudioProcessor: selected device has no input channels");
        return false;
    }
    selectedDeviceName_ = deviceInfo->name ? deviceInfo->name : "Unknown";
    std::string selectedDeviceLog = std::string("AudioProcessor: selected device: ") + selectedDeviceName_;
    debugLog(selectedDeviceLog.c_str());

    sampleRate_ = deviceInfo->defaultSampleRate > 0 ? deviceInfo->defaultSampleRate : kSampleRate;
    streamChannels_ = deviceInfo->maxInputChannels > 0 ? std::min(2, deviceInfo->maxInputChannels) : 1;
    sampleFrameSize_ = static_cast<int>(sampleRate_ / 30.0);
    fftSize_ = 1;
    while (fftSize_ < sampleFrameSize_) {
        fftSize_ <<= 1;
    }
    fftSize_ <<= 1;
    currentChunk_.clear();
    currentChunk_.reserve(sampleFrameSize_ * streamChannels_);
    pendingChunks_.clear();
    workerRunning_ = true;
    workerThread_ = std::thread(&AudioProcessor::workerLoop, this);
    {
        std::string infoLog = std::string("AudioProcessor: deviceIndex=") + std::to_string(deviceIndex) +
            ", maxInputChannels=" + std::to_string(deviceInfo->maxInputChannels) +
            ", maxOutputChannels=" + std::to_string(deviceInfo->maxOutputChannels) +
            ", defaultSampleRate=" + std::to_string(deviceInfo->defaultSampleRate) +
            ", openSampleRate=" + std::to_string(sampleRate_) +
            ", openChannels=" + std::to_string(streamChannels_) +
            ", frameSize=" + std::to_string(sampleFrameSize_) +
            ", fftSize=" + std::to_string(fftSize_);
        debugLog(infoLog.c_str());
    }

    PaStreamParameters inputParams;
    inputParams.device = deviceIndex;
    inputParams.channelCount = streamChannels_;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, &inputParams, nullptr, sampleRate_, kFrameSize, paClipOff, &AudioProcessor::audioCallback, this);
    if (err != paNoError) {
        std::string errLog = std::string("AudioProcessor: Pa_OpenStream failed: ") + Pa_GetErrorText(err);
        debugLog(errLog.c_str());
        return false;
    }
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::string errLog = std::string("AudioProcessor: Pa_StartStream failed: ") + Pa_GetErrorText(err);
        debugLog(errLog.c_str());
        return false;
    }
    debugLog("AudioProcessor: stream started successfully");
    return true;
}

bool AudioProcessor::start(int deviceIndex) {
    stop(); // Cleanly stop any active stream and join worker thread first

    if (deviceIndex == -99) {
        selectedDeviceName_ = "Virtual Keyboard Piano";
        selectedDeviceIndex_ = -99;
        return true;
    }

    // Native WASAPI Loopback Render Device (deviceIndex >= 1000)
    if (deviceIndex >= 1000) {
        int wasapiIdx = deviceIndex - 1000;
        bool ok = wasapiLoopback_.start(wasapiIdx, [this](const float* samples, unsigned long count, int channels, double sampleRate) {
            this->onWasapiAudio(samples, count, channels, sampleRate);
        });

        if (!ok) {
            debugLog("AudioProcessor: Native WASAPI loopback start failed");
            return false;
        }

        isWasapiLoopback_ = true;
        selectedDeviceName_ = wasapiLoopback_.currentDeviceName();
        selectedDeviceIndex_ = deviceIndex;
        std::string selectedDeviceLog = std::string("AudioProcessor: selected native WASAPI device: ") + selectedDeviceName_;
        debugLog(selectedDeviceLog.c_str());

        sampleRate_ = 48000.0;
        streamChannels_ = 2;
        sampleFrameSize_ = static_cast<int>(sampleRate_ / 30.0);
        fftSize_ = 1;
        while (fftSize_ < sampleFrameSize_) {
            fftSize_ <<= 1;
        }
        fftSize_ <<= 1;

        currentChunk_.clear();
        currentChunk_.reserve(sampleFrameSize_ * streamChannels_);
        pendingChunks_.clear();

        workerRunning_ = true;
        workerThread_ = std::thread(&AudioProcessor::workerLoop, this);
        return true;
    }

    // Standard PortAudio Input Device
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        debugLog("AudioProcessor: invalid device index for start(int)");
        return false;
    }

    if (deviceInfo->maxInputChannels == 0) {
        debugLog("AudioProcessor: target device has no input channels");
        return false;
    }

    selectedDeviceName_ = deviceInfo->name ? deviceInfo->name : "Unknown";
    selectedDeviceIndex_ = deviceIndex;
    std::string selectedDeviceLog = std::string("AudioProcessor: selected device: ") + selectedDeviceName_;
    debugLog(selectedDeviceLog.c_str());

    sampleRate_ = deviceInfo->defaultSampleRate > 0 ? deviceInfo->defaultSampleRate : kSampleRate;
    streamChannels_ = std::min(2, deviceInfo->maxInputChannels);
    if (streamChannels_ <= 0) streamChannels_ = 1;

    sampleFrameSize_ = static_cast<int>(sampleRate_ / 30.0);
    fftSize_ = 1;
    while (fftSize_ < sampleFrameSize_) {
        fftSize_ <<= 1;
    }
    fftSize_ <<= 1;

    currentChunk_.clear();
    currentChunk_.reserve(sampleFrameSize_ * streamChannels_);
    pendingChunks_.clear();

    PaStreamParameters inputParams;
    inputParams.device = deviceIndex;
    inputParams.channelCount = streamChannels_;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency > 0 ? deviceInfo->defaultLowInputLatency : 0.01;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, &inputParams, nullptr, sampleRate_, kFrameSize, paClipOff, &AudioProcessor::audioCallback, this);
    if (err != paNoError) {
        std::string errLog = std::string("AudioProcessor: Pa_OpenStream failed on device ") + std::to_string(deviceIndex) + ": " + Pa_GetErrorText(err);
        debugLog(errLog.c_str());
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::string errLog = std::string("AudioProcessor: Pa_StartStream failed: ") + Pa_GetErrorText(err);
        debugLog(errLog.c_str());
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    workerRunning_ = true;
    workerThread_ = std::thread(&AudioProcessor::workerLoop, this);
    debugLog("AudioProcessor: stream started successfully");
    return true;
}

void AudioProcessor::onWasapiAudio(const float* samples, unsigned long count, int channels, double sampleRate) {
    if (samples && count > 0) {
        sampleRate_ = sampleRate;
        streamChannels_ = channels;
        processInput(samples, count);
    }
}

std::vector<AudioProcessor::DeviceInfo> AudioProcessor::availableSources() const {
    std::vector<DeviceInfo> devices;

    // 1. Add Virtual Keyboard Piano option first
    DeviceInfo vkInfo;
    vkInfo.deviceIndex = -99;
    vkInfo.name = "Virtual Keyboard Piano";
    vkInfo.type = DeviceType::VirtualKeyboard;
    vkInfo.isDefault = false;
    devices.push_back(std::move(vkInfo));

    // 2. Add Native WASAPI Render Output Devices (Matching Windows Sound Configuration)
    auto renderDevices = WasapiLoopback::getRenderDevices();
    for (const auto& rDev : renderDevices) {
        DeviceInfo info;
        info.deviceIndex = 1000 + rDev.index;
        info.name = rDev.name;
        info.type = DeviceType::LoopbackOutput;
        info.isDefault = rDev.isDefault;
        devices.push_back(std::move(info));
    }

    // 3. Add PortAudio Input Devices (Microphones / Line-In)
    int defaultInput = Pa_GetDefaultInputDevice();
    int deviceCount = Pa_GetDeviceCount();
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (!deviceInfo || deviceInfo->maxInputChannels == 0) continue;

        DeviceInfo info;
        info.deviceIndex = i;
        info.name = deviceInfo->name ? deviceInfo->name : "Unknown Input";
        info.type = DeviceType::Microphone;
        info.isDefault = (i == defaultInput);
        devices.push_back(std::move(info));
    }

    return devices;
}

bool AudioProcessor::hasDevice(Source source) const {
    if (source == Source::Microphone) {
        return hasInputDevice();
    }
    return !WasapiLoopback::getRenderDevices().empty();
}

const std::string& AudioProcessor::selectedDeviceName() const {
    return selectedDeviceName_;
}

bool AudioProcessor::hasInputDevice() const {
    int defaultDevice = Pa_GetDefaultInputDevice();
    if (defaultDevice != paNoDevice) {
        return true;
    }

    int deviceCount = Pa_GetDeviceCount();
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            return true;
        }
    }
    return false;
}

bool AudioProcessor::findWasapiLoopbackDevice(int& deviceIndex) const {
    return false;
}

int AudioProcessor::findMatchingWasapiLoopbackDevice(int renderDeviceIndex) const {
    return paNoDevice;
}

void AudioProcessor::stop() {
    wasapiLoopback_.stop();
    isWasapiLoopback_ = false;

    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }

    workerRunning_ = false;
    chunkCv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

int AudioProcessor::audioCallback(const void* input, void* output,
                                  unsigned long frameCount,
                                  const PaStreamCallbackTimeInfo* timeInfo,
                                  PaStreamCallbackFlags statusFlags,
                                  void* userData) {
    auto* processor = static_cast<AudioProcessor*>(userData);
    if (!input) {
        debugLog("AudioProcessor: audio callback received null input");
    } else {
        std::string frameLog = std::string("AudioProcessor: audio callback frameCount=") + std::to_string(frameCount);
        debugLog(frameLog.c_str());
        processor->processInput(static_cast<const float*>(input), frameCount);
    }
    return paContinue;
}

void AudioProcessor::processInput(const float* input, unsigned long frameCount) {
    {
        std::lock_guard<std::mutex> lock(waveformMutex_);
        for (unsigned long i = 0; i < frameCount && i < waveform_.size(); ++i) {
            waveform_[i] = input[i * streamChannels_];
        }
    }

    std::vector<float> chunk(frameCount);
    if (streamChannels_ == 1) {
        for (unsigned long i = 0; i < frameCount; ++i) {
            chunk[i] = input[i];
        }
    } else {
        for (unsigned long i = 0; i < frameCount; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < streamChannels_; ++ch) {
                sum += input[i * streamChannels_ + ch];
            }
            chunk[i] = sum / static_cast<float>(streamChannels_);
        }
    }

    {
        std::lock_guard<std::mutex> lock(chunkMutex_);
        currentChunk_.insert(currentChunk_.end(), chunk.begin(), chunk.end());
        if (static_cast<int>(currentChunk_.size()) >= sampleFrameSize_) {
            std::vector<float> readyChunk(currentChunk_.begin(), currentChunk_.begin() + sampleFrameSize_);
            pendingChunks_.push_back(std::move(readyChunk));
            currentChunk_.erase(currentChunk_.begin(), currentChunk_.begin() + sampleFrameSize_);
            chunkCv_.notify_one();
        }
    }
}

void AudioProcessor::computeSpectrum(const std::vector<float>& chunk) {
    size_t chunkSamples = chunk.size();
    std::vector<float> real(fftSize_);
    std::vector<float> imag(fftSize_);
    std::fill(real.begin(), real.end(), 0.0f);
    for (size_t i = 0; i < chunkSamples && i < real.size(); ++i) {
        real[i] = chunk[i];
    }
    std::fill(imag.begin(), imag.end(), 0.0f);

    applyHannWindow(real.data(), fftSize_);
    fft(real.data(), imag.data(), fftSize_);

    std::vector<float> magnitudes(fftSize_ / 2);
    computeMagnitude(real.data(), imag.data(), magnitudes.data(), fftSize_ / 2);

    SpectrumArray bins;
    bins.fill(0.0f);

    float binResolution = static_cast<float>(sampleRate_) / static_cast<float>(fftSize_);

    for (int i = 1; i < fftSize_ / 2; ++i) {
        float freq = static_cast<float>(i) * binResolution;
        if (freq < kMinFrequency || freq > kMaxFrequency) continue;

        float centerBinFloat = freq - static_cast<float>(kMinFrequency);
        int baseBin = static_cast<int>(std::floor(centerBinFloat));
        float mag = magnitudes[i];

        // Spread magnitude smoothly over neighboring 1Hz bins to create a continuous spectral envelope
        int radius = static_cast<int>(std::ceil(binResolution));
        for (int offset = -radius; offset <= radius; ++offset) {
            int targetBin = baseBin + offset;
            if (targetBin >= 0 && targetBin < kBinCount) {
                float targetFreq = static_cast<float>(targetBin + kMinFrequency);
                float dist = std::abs(targetFreq - freq);
                if (dist <= binResolution) {
                    float weight = 1.0f - (dist / binResolution);
                    bins[targetBin] += mag * weight;
                }
            }
        }
    }

    float maxValue = 0.0f;
    for (float v : bins) maxValue = std::max(maxValue, v);

    // Absolute reference floor: prevents amplifying silent noise floor up to 1.0
    float normFactor = std::max(0.08f, maxValue);
    for (float& v : bins) {
        v /= normFactor;
    }

    {
        std::lock_guard<std::mutex> lock(spectrumMutex_);
        workingSpectrum_ = bins;
        newData_ = true;
    }
}

void AudioProcessor::workerLoop() {
    workerRunning_ = true;
    while (workerRunning_) {
        std::vector<float> chunk;
        {
            std::unique_lock<std::mutex> lock(chunkMutex_);
            chunkCv_.wait(lock, [this]() { return !pendingChunks_.empty() || !workerRunning_; });
            if (!workerRunning_) {
                break;
            }
            chunk = std::move(pendingChunks_.front());
            pendingChunks_.pop_front();
        }
        if (!chunk.empty()) {
            computeSpectrum(chunk);
        }
    }
}

void AudioProcessor::pollSpectrum(SpectrumArray& outSpectrum) {
    if (newData_) {
        std::lock_guard<std::mutex> lock(spectrumMutex_);
        outSpectrum = workingSpectrum_;
        newData_ = false;
    }
}

void AudioProcessor::pollWaveform(WaveformArray& outWaveform) {
    std::lock_guard<std::mutex> lock(waveformMutex_);
    outWaveform = waveform_;
}
