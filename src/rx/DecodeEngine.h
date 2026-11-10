#pragma once

#include "rx/AudioRingBuffer.h"
#include "rx/OnnxCwModel.h"
#include "rx/StreamingTranscript.h"

#include <QMutex>
#include <QString>
#include <QThread>
#include <QVector>
#include <atomic>
#include <cstdint>

namespace deepcw {

/**
 * Decode worker: snapshot ring → spectrum; streaming transcript → ORT/CTC.
 * ORT/STFT run only on this thread.
 */
class DecodeEngine : public QThread
{
    Q_OBJECT

public:
    explicit DecodeEngine(AudioRingBuffer *ring, QObject *parent = nullptr);
    ~DecodeEngine() override;

    bool loadModel(const QString &modelPath, QString *errorOut = nullptr);

    /// Feed resampled mono audio into the streaming commit buffer (from capture tap).
    void appendStreamingAudio(const float *samples, int count);
    void resetStreaming();
    void clearStreamingPending();

public slots:
    void setWindowSeconds(int seconds);
    void setCenterHz(int hz);
    void setBandwidthHz(int hz);
    void setSpectrumPaused(bool paused);
    void requestDecodeWake();
    void requestStop();

signals:
    /// Committed transcript + live pending tail (Phase RX-3).
    void transcriptUpdated(const QString &committed, const QString &pending);
    void rxStatusChanged(const QString &status);
    void decodeFailed(const QString &error);
    /// Magnitudes for display bins ~100–1500 Hz (low→high index).
    void spectrumUpdated(const QVector<float> &magnitudes);

protected:
    void run() override;

private:
    QVector<float> computeDisplaySpectrum(const std::vector<float> &audio) const;

    AudioRingBuffer *m_ring = nullptr;
    OnnxCwModel m_model;
    StreamingTranscript m_streaming;

    QMutex m_paramMutex;
    int m_windowSeconds = 12;
    int m_centerHz = 800;
    int m_bandwidthHz = 800;

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_wake{false};
    std::atomic<bool> m_spectrumPaused{false};
};

} // namespace deepcw
