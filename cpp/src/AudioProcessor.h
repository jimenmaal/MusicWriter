#pragma once

#include <QObject>
#include <QVector>

#ifdef USE_FFTW
#include <fftw3.h>
#endif

class QAudioInput;
class QIODevice;

class AudioProcessor : public QObject
{
    Q_OBJECT
public:
    explicit AudioProcessor(QObject *parent = nullptr);
    ~AudioProcessor();
    bool start();
    void stop();

signals:
    void spectrumUpdated(const QVector<double> &freqs, const QVector<double> &mags);

private slots:
    void handleAudio();

private:
    QAudioInput *m_audioIn = nullptr;
    QIODevice *m_io = nullptr;
    QVector<double> m_buffer;
    int m_sampleRate = 44100;
    int m_fftSize = 4096;
    void processFrame();
    void fft(const QVector<double> &in, QVector<std::complex<double>> &out);

#ifdef USE_FFTW
    // FFTW resources
    fftw_plan m_plan = nullptr;
    double *m_fftw_in = nullptr;
    fftw_complex *m_fftw_out = nullptr;
#endif
};
