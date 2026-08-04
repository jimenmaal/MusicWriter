#include "MainWindow.h"
#include "SpectrumWidget.h"
#include "AudioProcessor.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    m_title = new QLabel("frequency histogram", this);
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_spectrum = new SpectrumWidget(this);
    layout->addWidget(m_spectrum, 1);

    m_button = new QPushButton("Start audio record", this);
    connect(m_button, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    layout->addWidget(m_button);

    setCentralWidget(central);
    setWindowTitle("MusicWriter — Frequency Histogram");
    resize(800, 480);

    m_audio = new AudioProcessor(this);
    connect(m_audio, &AudioProcessor::spectrumUpdated, this, &MainWindow::onSpectrumUpdated);
}

MainWindow::~MainWindow() {}

void MainWindow::toggleRecording()
{
    if (!m_running) {
        if (m_audio->start()) {
            m_button->setText("Stop recording");
            m_running = true;
        } else {
            m_button->setText("Error: cannot start audio");
        }
    } else {
        m_audio->stop();
        m_button->setText("Start audio record");
        m_running = false;
    }
}

void MainWindow::onSpectrumUpdated(const QVector<double> &freqs, const QVector<double> &mags)
{
    m_spectrum->setData(freqs, mags);
}
