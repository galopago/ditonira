#include "rx/RxSpectrumWidget.h"

#include "rx/DeepCwConstants.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace deepcw {

RxSpectrumWidget::RxSpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    ensureImage();
}

void RxSpectrumWidget::setCenterHz(int hz)
{
    hz = qBound(hz, kDecodableMinFreqHz, kDecodableMaxFreqHz);
    if (m_centerHz == hz) {
        return;
    }
    m_centerHz = hz;
    update();
}

void RxSpectrumWidget::setBandwidthHz(int hz)
{
    if (m_bandwidthHz == hz) {
        return;
    }
    m_bandwidthHz = hz;
    update();
}

void RxSpectrumWidget::clearWaterfall()
{
    if (!m_image.isNull()) {
        m_image.fill(QColor(8, 10, 18));
    }
    m_smoothMax = 1e-3f;
    update();
}

void RxSpectrumWidget::ensureImage()
{
    const QRect pr = plotRect();
    const int w = std::max(1, pr.width());
    const int h = std::max(1, pr.height());
    if (m_image.width() == w && m_image.height() == h) {
        return;
    }
    QImage next(w, h, QImage::Format_RGB32);
    next.fill(QColor(8, 10, 18));
    if (!m_image.isNull()) {
        // Preserve recent columns (right-aligned).
        const int copyW = std::min(m_image.width(), w);
        const int copyH = std::min(m_image.height(), h);
        QPainter p(&next);
        p.drawImage(QRect(w - copyW, h - copyH, copyW, copyH),
                    m_image,
                    QRect(m_image.width() - copyW, m_image.height() - copyH, copyW, copyH));
    }
    m_image = next;
}

QRect RxSpectrumWidget::plotRect() const
{
    return QRect(kLeftAxis, 4, std::max(1, width() - kLeftAxis - 4),
                 std::max(1, height() - kBottomAxis - 4));
}

int RxSpectrumWidget::yToHz(int y) const
{
    const QRect pr = plotRect();
    if (pr.height() <= 1) {
        return kMinHz;
    }
    const double t = 1.0 - static_cast<double>(y - pr.top()) / static_cast<double>(pr.height() - 1);
    return static_cast<int>(std::lround(kMinHz + t * (kMaxHz - kMinHz)));
}

int RxSpectrumWidget::hzToY(int hz) const
{
    const QRect pr = plotRect();
    const double t = (static_cast<double>(hz) - kMinHz) / static_cast<double>(kMaxHz - kMinHz);
    const double clamped = std::min(1.0, std::max(0.0, t));
    return pr.top() + static_cast<int>(std::lround((1.0 - clamped) * (pr.height() - 1)));
}

QColor RxSpectrumWidget::magnitudeToColor(float mag, float maxMag) const
{
    const float norm = std::min(1.f, std::max(0.f, mag / std::max(maxMag, 1e-9f)));
    // Dark blue → cyan → yellow → white (readable contrast).
    if (norm < 0.25f) {
        const float u = norm / 0.25f;
        return QColor(static_cast<int>(8 + 20 * u), static_cast<int>(10 + 40 * u),
                      static_cast<int>(18 + 100 * u));
    }
    if (norm < 0.55f) {
        const float u = (norm - 0.25f) / 0.30f;
        return QColor(static_cast<int>(28 + 20 * u), static_cast<int>(50 + 140 * u),
                      static_cast<int>(118 + 80 * u));
    }
    if (norm < 0.8f) {
        const float u = (norm - 0.55f) / 0.25f;
        return QColor(static_cast<int>(48 + 180 * u), static_cast<int>(190 + 40 * u),
                      static_cast<int>(198 - 150 * u));
    }
    const float u = (norm - 0.8f) / 0.20f;
    return QColor(static_cast<int>(228 + 27 * u), static_cast<int>(230 + 25 * u),
                  static_cast<int>(48 + 200 * u));
}

void RxSpectrumWidget::appendSpectrumColumn(const QVector<float> &magnitudes)
{
    if (magnitudes.isEmpty()) {
        return;
    }
    ensureImage();
    if (m_image.width() < 2 || m_image.height() < 2) {
        return;
    }

    float peak = 0.f;
    for (float m : magnitudes) {
        peak = std::max(peak, m);
    }
    m_smoothMax = std::max(peak, m_smoothMax * 0.98f + peak * 0.02f);
    m_smoothMax = std::max(m_smoothMax, 1e-4f);

    // Scroll left by 1 px; draw new column on the right (time →).
    QImage next(m_image.width(), m_image.height(), QImage::Format_RGB32);
    next.fill(QColor(8, 10, 18));
    {
        QPainter p(&next);
        p.drawImage(-1, 0, m_image);
    }

    const int x = next.width() - 1;
    const int h = next.height();
    const int n = magnitudes.size();
    for (int y = 0; y < h; ++y) {
        // Map widget Y (top=high freq) to magnitude index (0=low freq).
        const double t = 1.0 - static_cast<double>(y) / static_cast<double>(std::max(1, h - 1));
        const int idx = std::min(n - 1, std::max(0, static_cast<int>(std::lround(t * (n - 1)))));
        next.setPixelColor(x, y, magnitudeToColor(magnitudes[idx], m_smoothMax));
    }
    m_image = next;
    update();
}

void RxSpectrumWidget::applyCenterFromPos(const QPoint &pos)
{
    if (!plotRect().contains(pos)) {
        return;
    }
    int hz = yToHz(pos.y());
    hz = qBound(hz, kDecodableMinFreqHz, kDecodableMaxFreqHz);
    if (hz != m_centerHz) {
        m_centerHz = hz;
        emit centerHzSelected(hz);
        update();
    }
}

void RxSpectrumWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        applyCenterFromPos(event->pos());
    }
    QWidget::mousePressEvent(event);
}

void RxSpectrumWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        applyCenterFromPos(event->pos());
    }
    QWidget::mouseMoveEvent(event);
}

void RxSpectrumWidget::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        return;
    }
    // Wheel up → higher frequency.
    const int hz = qBound(m_centerHz + steps * static_cast<int>(kBinResolution),
                        kDecodableMinFreqHz, kDecodableMaxFreqHz);
    if (hz != m_centerHz) {
        m_centerHz = hz;
        emit centerHzSelected(hz);
        update();
    }
    event->accept();
}

void RxSpectrumWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ensureImage();
}

void RxSpectrumWidget::paintEvent(QPaintEvent *)
{
    ensureImage();
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 22, 28));

    const QRect pr = plotRect();
    p.drawImage(pr, m_image);

    // Frequency axis labels.
    p.setPen(QColor(180, 185, 195));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);
    for (int hz : {100, 400, 800, 1200, 1500}) {
        const int y = hzToY(hz);
        p.drawText(QRect(0, y - 7, kLeftAxis - 4, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(hz));
        p.setPen(QColor(60, 65, 80));
        p.drawLine(pr.left(), y, pr.right(), y);
        p.setPen(QColor(180, 185, 195));
    }
    p.drawText(QRect(pr.left(), height() - kBottomAxis, pr.width(), kBottomAxis),
               Qt::AlignCenter, tr("Time →"));

    // Bandwidth band + center line overlay.
    const int yLo = hzToY(m_centerHz - m_bandwidthHz / 2);
    const int yHi = hzToY(m_centerHz + m_bandwidthHz / 2);
    const int top = std::min(yLo, yHi);
    const int bot = std::max(yLo, yHi);
    p.fillRect(QRect(pr.left(), top, pr.width(), std::max(1, bot - top)),
               QColor(80, 160, 255, 40));
    p.setPen(QPen(QColor(255, 220, 80), 1, Qt::DashLine));
    const int yC = hzToY(m_centerHz);
    p.drawLine(pr.left(), yC, pr.right(), yC);

    p.setPen(QColor(100, 110, 130));
    p.drawRect(pr.adjusted(0, 0, -1, -1));
}

} // namespace deepcw
