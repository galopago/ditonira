#pragma once

#include "rx/AudioRingBuffer.h"
#include "rx/OnnxCwModel.h"
#include "rx/PileupTracker.h"
#include "rx/StreamingTranscript.h"

#include <QMutex>
#include <QString>
#include <QThread>
#include <QVector>
#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace deepcw {

struct PileupTrackText {
    int id = 0;
    int frequencyHz = 0;
    QString text;
};

/**
 * Decode worker: spectrum + Normal streaming transcript OR Pileup narrow tracks.
 * ORT/STFT run only on this thread.
 */
class DecodeEngine : public QThread
{
    Q_OBJECT

public:
    enum class Mode { Normal, Pileup };

    explicit DecodeEngine(AudioRingBuffer *ring, QObject *parent = nullptr);
    ~DecodeEngine() override;

    bool loadModels(const QString &wideModelPath, const QString &narrowModelPath,
                    QString *errorOut = nullptr);

    void resetStreaming();
    void clearStreamingPending();
    void resetPileup();

public slots:
    void setMode(deepcw::DecodeEngine::Mode mode);
    void setWindowSeconds(int seconds);
    void setCenterHz(int hz);
    void setBandwidthHz(int hz);
    void setSpectrumPaused(bool paused);
    void requestDecodeWake();
    void requestStop();

signals:
    void transcriptUpdated(const QString &committed, const QString &pending);
    void pileupUpdated(const QVector<deepcw::PileupTrackText> &tracks);
    void rxStatusChanged(const QString &status);
    void decodeFailed(const QString &error);
    void spectrumUpdated(const QVector<float> &magnitudes);

protected:
    void run() override;

private:
    QVector<float> computeDisplaySpectrum(const std::vector<float> &audio) const;
    void runNormalTick(float filterFreq, float filterWidth);
    void runPileupTick(const std::vector<float> &audio);

    AudioRingBuffer *m_ring = nullptr;
    OnnxCwModel m_wideModel;
    OnnxCwModel m_narrowModel;
    StreamingTranscript m_streaming;
    PileupTracker m_pileupTracker;

    QMutex m_paramMutex;
    Mode m_mode = Mode::Normal;
    int m_windowSeconds = 12;
    int m_centerHz = 800;
    int m_bandwidthHz = 800;

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_wake{false};
    std::atomic<bool> m_spectrumPaused{false};

    std::unordered_map<int, QString> m_pileupText;
    std::unordered_map<int, std::int64_t> m_pileupLastNonEmptyMs;
    std::vector<float> m_pileupLockedHz;
    std::uint64_t m_streamCursor = 0;
    std::vector<float> m_streamScratch;
};

} // namespace deepcw

Q_DECLARE_METATYPE(deepcw::PileupTrackText)
Q_DECLARE_METATYPE(QVector<deepcw::PileupTrackText>)
