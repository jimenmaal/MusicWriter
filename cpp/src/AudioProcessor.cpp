#include "AudioProcessor.h"
#include <QAudioInput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QIODevice>
#include <QDebug>
#include <complex>
#include <cmath>

AudioProcessor::AudioProcessor(QObject *parent)
    : QObject(parent)
{
    m_buffer.reserve(m_fftSize * 2);
#ifdef USE_FFTW
    m_fftw_in = (double*)fftw_malloc(sizeof(double) * m_fftSize);
    m_fftw_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (m_fftSize/2 + 1));
    if (m_fftw_in && m_fftw_out) {
        m_plan = fftw_plan_dft_r2c_1d(m_fftSize, m_fftw_in, m_fftw_out, FFTW_ESTIMATE);
    }
#endif
}

AudioProcessor::~AudioProcessor()
{
    stop();
#ifdef USE_FFTW
    if (m_plan) fftw_destroy_plan(m_plan);
    if (m_fftw_in) fftw_free(m_fftw_in);
    if (m_fftw_out) fftw_free(m_fftw_out);
#endif
}

bool AudioProcessor::start()
{
    if (m_audioIn) return true;

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(format)) {
        format = info.nearestFormat(format);
    }

    m_audioIn = new QAudioInput(format, this);
    m_io = m_audioIn->start();
    if (!m_io) {
        delete m_audioIn;
        m_audioIn = nullptr;
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioProcessor::handleAudio);
    return true;
}

void AudioProcessor::stop()
{
    if (m_audioIn) {
        m_audioIn->stop();
        m_io = nullptr;
        delete m_audioIn;
        m_audioIn = nullptr;
    }
    m_buffer.clear();
}

void AudioProcessor::handleAudio()
{
    if (!m_io) return;
    QByteArray data = m_io->readAll();
    // assume 16-bit signed samples
    const int16_t *samples = reinterpret_cast<const int16_t*>(data.constData());
    int sampleCount = data.size() / sizeof(int16_t);
    for (int i = 0; i < sampleCount; ++i) {
        double v = samples[i] / 32768.0;
        m_buffer.append(v);
    }

    // use 50% overlap for smoother display
    int hopSize = m_fftSize / 2;
    while (m_buffer.size() >= m_fftSize) {
        processFrame();
        // remove hopSize samples to create overlap
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + hopSize);
    }
}

void AudioProcessor::processFrame()
{
    QVector<double> windowed(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) {
        double w = 0.5 * (1 - std::cos(2 * M_PI * i / (m_fftSize - 1))); // Hann
        windowed[i] = m_buffer[i] * w;
    }

    QVector<std::complex<double>> out(m_fftSize);

#ifdef USE_FFTW
    if (m_plan && m_fftw_in && m_fftw_out) {
        for (int i = 0; i < m_fftSize; ++i) m_fftw_in[i] = windowed[i];
        fftw_execute(m_plan);
        int half = m_fftSize / 2 + 1;
        QVector<double> freqs;
        QVector<double> mags;
        freqs.reserve(half);
        mags.reserve(half);
        for (int k = 0; k < half; ++k) {
            double freq = double(k) * m_sampleRate / m_fftSize;
            if (freq < 250.0 || freq > 6000.0) continue;
            double mag = std::hypot(m_fftw_out[k][0], m_fftw_out[k][1]);
            freqs.push_back(freq);
            mags.push_back(mag);
        }
        emit spectrumUpdated(freqs, mags);
        return;
    }
#endif

    // fallback to internal FFT implementation
    fft(windowed, out);

    int half = m_fftSize / 2 + 1;
    QVector<double> freqs;
    QVector<double> mags;
    freqs.reserve(half);
    mags.reserve(half);
    for (int k = 0; k < half; ++k) {
        double freq = double(k) * m_sampleRate / m_fftSize;
        if (freq < 250.0 || freq > 6000.0) continue;
        double mag = std::abs(out[k]);
        freqs.push_back(freq);
        mags.push_back(mag);
    }

    emit spectrumUpdated(freqs, mags);
}

// naive iterative Cooley-Tukey FFT (in-place) using complex vectors
static void bitReverseSwap(QVector<std::complex<double>> &a)
{
    int n = a.size();
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}

void AudioProcessor::fft(const QVector<double> &in, QVector<std::complex<double>> &out)
{
    int n = in.size();
    out.resize(n);
    for (int i = 0; i < n; ++i) out[i] = std::complex<double>(in[i], 0.0);

    bitReverseSwap(out);

    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len/2; ++j) {
                std::complex<double> u = out[i + j];
                std::complex<double> v = out[i + j + len/2] * w;
                out[i + j] = u + v;
                out[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
}
