#pragma once

#include <QVector>
#include <QWidget>
#include <QImage>

namespace deepcw {

/**
 * Live waterfall for RX debugging (ARCHITECTURE_RX_DEEPCW.md §7.3).
 * Orientation: X = time (newest on the right), Y = frequency (100 Hz bottom → 1500 Hz top).
 */
class RxSpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RxSpectrumWidget(QWidget *parent = nullptr);

    void setCenterHz(int hz);
    void setBandwidthHz(int hz);
    void clearWaterfall();

public slots:
    /// One magnitude column for display bins covering ~100–1500 Hz (linear power).
    void appendSpectrumColumn(const QVector<float> &magnitudes);

signals:
    void centerHzSelected(int hz);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRect plotRect() const;
    int yToHz(int y) const;
    int hzToY(int hz) const;
    QColor magnitudeToColor(float mag, float maxMag) const;
    void ensureImage();
    void applyCenterFromPos(const QPoint &pos);

    QImage m_image;
    int m_centerHz = 800;
    int m_bandwidthHz = 800;
    float m_smoothMax = 1e-3f;

    static constexpr int kMinHz = 100;
    static constexpr int kMaxHz = 1500;
    static constexpr int kLeftAxis = 48;
    static constexpr int kBottomAxis = 18;
};

} // namespace deepcw
