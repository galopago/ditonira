/**
 * Phase RX-1 offline harness: WAV → 9600 Hz → STFT/spectrogram → ONNX → CTC plainText.
 *
 * Usage:
 *   deepcw_wav_decode [--model path] [--bandpass centerHz widthHz] <wav>
 *
 * Default model: ../share/ditonira/models/deepcw_en.onnx relative to the binary,
 * falling back to resources/models/deepcw_en.onnx from the source/build layout.
 */

#include "rx/CtcDecoder.h"
#include "rx/DeepCwConstants.h"
#include "rx/OnnxCwModel.h"
#include "rx/SpectrogramBuilder.h"
#include "rx/WavIo.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string defaultModelPath(const std::string &argv0)
{
    // Prefer build-tree relative: <build>/../resources/models/deepcw_en.onnx
    // Also try installed share path next to binary.
    const auto tryPaths = {
        std::string("resources/models/deepcw_en.onnx"),
        std::string("../resources/models/deepcw_en.onnx"),
        std::string("../../resources/models/deepcw_en.onnx"),
        std::string("../share/ditonira/models/deepcw_en.onnx"),
        std::string("share/ditonira/models/deepcw_en.onnx"),
    };
    for (const auto &p : tryPaths) {
        FILE *f = std::fopen(p.c_str(), "rb");
        if (f) {
            std::fclose(f);
            return p;
        }
    }
    (void)argv0;
    return "resources/models/deepcw_en.onnx";
}

} // namespace

int main(int argc, char **argv)
{
    std::string modelPath;
    std::string wavPath;
    float filterFreq = -1.f;
    float filterWidth = 0.f;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "--bandpass" && i + 2 < argc) {
            filterFreq = std::stof(argv[++i]);
            filterWidth = std::stof(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: deepcw_wav_decode [--model path] "
                         "[--bandpass centerHz widthHz] <wav>\n";
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            wavPath = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 2;
        }
    }

    if (wavPath.empty()) {
        std::cerr << "Usage: deepcw_wav_decode [--model path] <wav>\n";
        return 2;
    }
    if (modelPath.empty()) {
        modelPath = defaultModelPath(argv[0] ? argv[0] : "");
    }

    deepcw::WavData wav;
    std::string err;
    if (!deepcw::readWavFile(wavPath, &wav, &err)) {
        std::cerr << "WAV error: " << err << "\n";
        return 1;
    }

    std::vector<float> audio = deepcw::resampleLinear(
        wav.samples.data(), static_cast<int>(wav.samples.size()), wav.sampleRate,
        deepcw::kSampleRate);

    if (static_cast<int>(audio.size()) <= deepcw::kCenterPad + 1) {
        std::cerr << "Audio too short after resample to 9600 Hz\n";
        return 1;
    }

    auto spectrogram =
        deepcw::audioToSpectrogramTensor(audio.data(), static_cast<int>(audio.size()),
                                         filterFreq, filterWidth);
    if (!spectrogram) {
        std::cerr << "Spectrogram build failed (audio too short?)\n";
        return 1;
    }

    deepcw::OnnxCwModel model;
    if (!model.load(modelPath, &err)) {
        std::cerr << "Could not load CW model (" << modelPath << "): " << err << "\n";
        return 1;
    }

    deepcw::InferenceResult infer;
    if (!model.run(*spectrogram, &infer, &err)) {
        std::cerr << "Inference failed: " << err << "\n";
        return 1;
    }

    const auto decoded =
        deepcw::decodeEnLogProbs(infer.logProbs.data(), infer.timeSteps, infer.numClasses);

    std::cout << decoded.plainText << "\n";
    return 0;
}
