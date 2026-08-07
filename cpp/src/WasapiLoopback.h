#ifndef WASAPILOOPBACK_H
#define WASAPILOOPBACK_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

struct WasapiDeviceInfo {
    int index = -1;
    std::string id;
    std::string name;
    bool isDefault = false;
};

class WasapiLoopback {
public:
    using AudioCallbackFn = std::function<void(const float* samples, unsigned long count, int channels, double sampleRate)>;

    WasapiLoopback();
    ~WasapiLoopback();

    static std::vector<WasapiDeviceInfo> getRenderDevices();
    bool start(int deviceIndex, AudioCallbackFn callback);
    void stop();
    bool isRunning() const { return running_; }
    const std::string& currentDeviceName() const { return currentDeviceName_; }

private:
    void captureLoop();

    std::atomic<bool> running_{false};
    std::thread captureThread_;
    AudioCallbackFn callback_;
    std::string currentDeviceName_;
    double sampleRate_ = 48000.0;
    int channels_ = 2;

#if defined(_WIN32)
    IMMDevice* pDevice_ = nullptr;
    IAudioClient* pAudioClient_ = nullptr;
    IAudioCaptureClient* pCaptureClient_ = nullptr;
    WAVEFORMATEX* pWaveFormat_ = nullptr;
#endif
};

#endif // WASAPILOOPBACK_H
