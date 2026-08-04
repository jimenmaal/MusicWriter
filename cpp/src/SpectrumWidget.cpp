#include "SpectrumWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
}

void SpectrumWidget::setData(const QVector<double> &freqs, const QVector<double> &mags)
{
    m_freqs = freqs;
    m_mags = mags;
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_freqs.isEmpty() || m_mags.isEmpty()) return;

    int w = width();
    int h = height();

    // find magnitude range
    double minv = *std::min_element(m_mags.constBegin(), m_mags.constEnd());
    double maxv = *std::max_element(m_mags.constBegin(), m_mags.constEnd());
    if (minv == maxv) maxv = minv + 1.0;

    int n = m_freqs.size();
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::green);

    for (int i = 0; i < n; ++i) {
        double x = double(i) / n * w;
        double val = (m_mags[i] - minv) / (maxv - minv);
        double barh = val * h;
        QRectF r(x, h - barh, std::max(1.0, double(w) / n), barh);
        p.drawRect(r);
    }
}
