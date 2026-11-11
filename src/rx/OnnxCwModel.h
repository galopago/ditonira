#pragma once

#include "rx/SpectrogramBuilder.h"

#include <memory>
#include <string>
#include <vector>

namespace deepcw {

struct InferenceResult {
    std::vector<float> logProbs; // [T_out * 42]
    int timeSteps = 0;
    int numClasses = 42;
};

/**
 * ONNX Runtime wrapper for EN Conformer-CTC (wide 65-bin or narrow 3-bin).
 * Input name: spectrogram [1,1,T,F]; output name: log_probs [1,T_out,42].
 */
class OnnxCwModel
{
public:
    OnnxCwModel();
    ~OnnxCwModel();

    OnnxCwModel(const OnnxCwModel &) = delete;
    OnnxCwModel &operator=(const OnnxCwModel &) = delete;

    bool load(const std::string &modelPath, std::string *errorOut = nullptr);
    bool isLoaded() const;

    bool run(const SpectrogramTensor &spectrogram, InferenceResult *out,
             std::string *errorOut = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace deepcw
