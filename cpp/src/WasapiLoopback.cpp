#include "WasapiLoopback.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <Functiondiscoverykeys_devpkey.h>

#if defined(_WIN32)
#pragma comment(lib, "ole32.lib")
#endif

namespace {
    std::string wcharToString(const wchar_t* wstr) {
        if (!wstr) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
        if (size_needed <= 0) return "";
        std::string str(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed, NULL, NULL);
        return str;
    }
}

WasapiLoopback::WasapiLoopback() {
}

WasapiLoopback::~WasapiLoopback() {
    stop();
}

std::vector<WasapiDeviceInfo> WasapiLoopback::getRenderDevices() {
    std::vector<WasapiDeviceInfo> result;
#if defined(_WIN32)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool uninit = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        if (uninit) CoUninitialize();
        return result;
    }

    IMMDevice* pDefaultDevice = nullptr;
    std::string defaultId;
    if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice)) && pDefaultDevice) {
        LPWSTR pstrId = nullptr;
        if (SUCCEEDED(pDefaultDevice->GetId(&pstrId)) && pstrId) {
            defaultId = wcharToString(pstrId);
            CoTaskMemFree(pstrId);
        }
        pDefaultDevice->Release();
    }

    IMMDeviceCollection* pCollection = nullptr;
    if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)) && pCollection) {
        UINT count = 0;
        pCollection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pCollection->Item(i, &pDevice)) && pDevice) {
                LPWSTR pstrId = nullptr;
                std::string devId;
                if (SUCCEEDED(pDevice->GetId(&pstrId)) && pstrId) {
                    devId = wcharToString(pstrId);
                    CoTaskMemFree(pstrId);
                }

                IPropertyStore* pProps = nullptr;
                std::string friendlyName = "Unknown Output";
                if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
                        friendlyName = wcharToString(varName.pwszVal);
                    }
                    PropVariantClear(&varName);
                    pProps->Release();
                }

                WasapiDeviceInfo info;
                info.index = static_cast<int>(i);
                info.id = devId;
                info.name = friendlyName;
                info.isDefault = (!defaultId.empty() && devId == defaultId);
                result.push_back(std::move(info));

                pDevice->Release();
            }
        }
        pCollection->Release();
    }

    pEnumerator->Release();
    if (uninit) CoUninitialize();
#endif
    return result;
}

bool WasapiLoopback::start(int deviceIndex, AudioCallbackFn callback) {
    stop();

#if defined(_WIN32)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        return false;
    }

    IMMDeviceCollection* pCollection = nullptr;
    if (deviceIndex >= 0) {
        if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)) && pCollection) {
            pCollection->Item(static_cast<UINT>(deviceIndex), &pDevice_);
            pCollection->Release();
        }
    } else {
        pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice_);
    }

    pEnumerator->Release();

    if (!pDevice_) {
        return false;
    }

    // Retrieve Friendly Name
    IPropertyStore* pProps = nullptr;
    currentDeviceName_ = "WASAPI Output";
    if (SUCCEEDED(pDevice_->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
            currentDeviceName_ = wcharToString(varName.pwszVal);
        }
        PropVariantClear(&varName);
        pProps->Release();
    }

    hr = pDevice_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient_);
    if (FAILED(hr) || !pAudioClient_) {
        pDevice_->Release();
        pDevice_ = nullptr;
        return false;
    }

    hr = pAudioClient_->GetMixFormat(&pWaveFormat_);
    if (FAILED(hr) || !pWaveFormat_) {
        pAudioClient_->Release();
        pAudioClient_ = nullptr;
        pDevice_->Release();
        pDevice_ = nullptr;
        return false;
    }

    sampleRate_ = pWaveFormat_->nSamplesPerSec;
    channels_ = pWaveFormat_->nChannels;

    REFERENCE_TIME hnsBufferDuration = 10000000; // 1 second buffer
    hr = pAudioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsBufferDuration, 0, pWaveFormat_, NULL);
    if (FAILED(hr)) {
        CoTaskMemFree(pWaveFormat_);
        pWaveFormat_ = nullptr;
        pAudioClient_->Release();
        pAudioClient_ = nullptr;
        pDevice_->Release();
        pDevice_ = nullptr;
        return false;
    }

    hr = pAudioClient_->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient_);
    if (FAILED(hr) || !pCaptureClient_) {
        CoTaskMemFree(pWaveFormat_);
        pWaveFormat_ = nullptr;
        pAudioClient_->Release();
        pAudioClient_ = nullptr;
        pDevice_->Release();
        pDevice_ = nullptr;
        return false;
    }

    hr = pAudioClient_->Start();
    if (FAILED(hr)) {
        pCaptureClient_->Release();
        pCaptureClient_ = nullptr;
        CoTaskMemFree(pWaveFormat_);
        pWaveFormat_ = nullptr;
        pAudioClient_->Release();
        pAudioClient_ = nullptr;
        pDevice_->Release();
        pDevice_ = nullptr;
        return false;
    }

    callback_ = callback;
    running_ = true;
    captureThread_ = std::thread(&WasapiLoopback::captureLoop, this);
    return true;
#else
    return false;
#endif
}

void WasapiLoopback::stop() {
    running_ = false;
    if (captureThread_.joinable()) {
        captureThread_.join();
    }

#if defined(_WIN32)
    if (pAudioClient_) {
        pAudioClient_->Stop();
    }
    if (pCaptureClient_) {
        pCaptureClient_->Release();
        pCaptureClient_ = nullptr;
    }
    if (pAudioClient_) {
        pAudioClient_->Release();
        pAudioClient_ = nullptr;
    }
    if (pWaveFormat_) {
        CoTaskMemFree(pWaveFormat_);
        pWaveFormat_ = nullptr;
    }
    if (pDevice_) {
        pDevice_->Release();
        pDevice_ = nullptr;
    }
#endif
}

void WasapiLoopback::captureLoop() {
#if defined(_WIN32)
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    std::vector<float> floatSamples;

    while (running_) {
        UINT32 packetLength = 0;
        HRESULT hr = pCaptureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(hr) || packetLength == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        BYTE* pData = nullptr;
        UINT32 numFramesToRead = 0;
        DWORD flags = 0;

        hr = pCaptureClient_->GetBuffer(&pData, &numFramesToRead, &flags, NULL, NULL);
        if (SUCCEEDED(hr) && pData && numFramesToRead > 0) {
            size_t totalSamples = numFramesToRead * channels_;
            floatSamples.resize(totalSamples);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::fill(floatSamples.begin(), floatSamples.end(), 0.0f);
            } else {
                bool isFloat = false;
                if (pWaveFormat_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                    isFloat = true;
                } else if (pWaveFormat_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                    const WAVEFORMATEXTENSIBLE* pEx = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pWaveFormat_);
                    if (pEx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
                        isFloat = true;
                    }
                }

                if (isFloat) {
                    const float* fSrc = reinterpret_cast<const float*>(pData);
                    std::copy(fSrc, fSrc + totalSamples, floatSamples.begin());
                } else {
                    int bitsPerSample = pWaveFormat_->wBitsPerSample;
                    if (bitsPerSample == 16) {
                        const int16_t* sSrc = reinterpret_cast<const int16_t*>(pData);
                        for (size_t i = 0; i < totalSamples; ++i) {
                            floatSamples[i] = static_cast<float>(sSrc[i]) / 32768.0f;
                        }
                    } else if (bitsPerSample == 24) {
                        const uint8_t* bSrc = reinterpret_cast<const uint8_t*>(pData);
                        for (size_t i = 0; i < totalSamples; ++i) {
                            int32_t val = (bSrc[i * 3 + 0] << 8) | (bSrc[i * 3 + 1] << 16) | (bSrc[i * 3 + 2] << 24);
                            floatSamples[i] = static_cast<float>(val >> 8) / 8388608.0f;
                        }
                    } else if (bitsPerSample == 32) {
                        const int32_t* iSrc = reinterpret_cast<const int32_t*>(pData);
                        for (size_t i = 0; i < totalSamples; ++i) {
                            floatSamples[i] = static_cast<float>(iSrc[i]) / 2147483648.0f;
                        }
                    } else {
                        std::fill(floatSamples.begin(), floatSamples.end(), 0.0f);
                    }
                }
            }

            pCaptureClient_->ReleaseBuffer(numFramesToRead);

            if (callback_ && !floatSamples.empty()) {
                callback_(floatSamples.data(), numFramesToRead, channels_, sampleRate_);
            }
        }
    }

    CoUninitialize();
#endif
}
