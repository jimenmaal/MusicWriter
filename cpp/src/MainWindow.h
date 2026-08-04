#pragma once

#include <QMainWindow>

class QPushButton;
class QLabel;
class SpectrumWidget;
class AudioProcessor;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void toggleRecording();
    void onSpectrumUpdated(const QVector<double> &freqs, const QVector<double> &mags);

private:
    QLabel *m_title;
    SpectrumWidget *m_spectrum;
    QPushButton *m_button;
    AudioProcessor *m_audio;
    bool m_running = false;
};
