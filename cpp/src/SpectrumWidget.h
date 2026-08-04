#pragma once

#include <QWidget>
#include <QVector>

class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget *parent = nullptr);
    void setData(const QVector<double> &freqs, const QVector<double> &mags);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_freqs;
    QVector<double> m_mags;
};
