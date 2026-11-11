// Offline smoke for DEC-009 / RX-4 pileup path (no PortAudio callback).
#include "rx/CtcDecoder.h"
#include "rx/DeepCwConstants.h"
#include "rx/OnnxCwModel.h"
#include "rx/PileupTracker.h"
#include "rx/SpectrogramBuilder.h"
#include "rx/WavIo.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    const char *wavPath =
        argc > 1 ? argv[1] : "tests/rx_wav/pileup_4track_multispeed.wav";
    const char *modelPath =
        argc > 2 ? argv[2] : "resources/models/deepcw_en_narrow.onnx";

    deepcw::WavData wav;
    std::string err;
    if (!deepcw::readWavFile(wavPath, &wav, &err)) {
        std::fprintf(stderr, "WAV failed: %s\n", err.c_str());
        return 1;
    }
    if (wav.sampleRate != deepcw::kSampleRate) {
        wav.samples = deepcw::resampleLinear(wav.samples.data(),
                                             static_cast<int>(wav.samples.size()),
                                             wav.sampleRate, deepcw::kSampleRate);
        wav.sampleRate = deepcw::kSampleRate;
    }

    deepcw::OnnxCwModel model;
    if (!model.load(modelPath, &err)) {
        std::fprintf(stderr, "Model failed: %s\n", err.c_str());
        return 1;
    }

    const int window = deepcw::kPileupWindowS * deepcw::kSampleRate;
    const int total = static_cast<int>(wav.samples.size());
    deepcw::PileupTracker tracker;
    std::vector<float> locked;

    int hops = 0;
    for (int pos = 0; pos + window <= total; pos += deepcw::kSampleRate / 2) {
        const float *chunk = wav.samples.data() + pos;
        auto snr = deepcw::estimateCwBinSnr(chunk, window, deepcw::kPileupMinFreqHz,
                                            deepcw::kPileupMaxFreqHz);
        if (!snr) {
            continue;
        }
        auto raw = deepcw::detectCwBinCandidates(snr->frequenciesHz, snr->snrDb, locked);
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        auto tracks = tracker.update(raw, nowMs);
        locked.clear();
        for (const auto &t : tracks) {
            locked.push_back(static_cast<float>(t.frequencyHz));
            auto spec = deepcw::audioToNarrowShiftedSpectrogramTensor(
                chunk, window, static_cast<float>(t.frequencyHz));
            if (!spec) {
                continue;
            }
            deepcw::InferenceResult infer;
            if (!model.run(*spec, &infer, &err)) {
                std::fprintf(stderr, "ORT fail: %s\n", err.c_str());
                return 2;
            }
            const auto decoded =
                deepcw::decodeEnLogProbs(infer.logProbs.data(), infer.timeSteps,
                                         infer.numClasses);
            std::printf("hop=%d track=%d Hz=%d text=%s\n", hops, t.id, t.frequencyHz,
                        decoded.plainText.c_str());
        }
        ++hops;
        if (hops >= 8) {
            break;
        }
    }

    std::printf("OK pileup_wav_smoke hops=%d samples=%d\n", hops, total);
    return 0;
}
