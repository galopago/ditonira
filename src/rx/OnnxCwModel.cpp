#include "rx/OnnxCwModel.h"

#include "rx/DeepCwConstants.h"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <stdexcept>

namespace deepcw {

struct OnnxCwModel::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "ditonira-deepcw"};
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;
};

OnnxCwModel::OnnxCwModel()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_impl->sessionOptions.SetIntraOpNumThreads(1);
}

OnnxCwModel::~OnnxCwModel() = default;

bool OnnxCwModel::isLoaded() const
{
    return m_impl && m_impl->session != nullptr;
}

bool OnnxCwModel::load(const std::string &modelPath, std::string *errorOut)
{
    try {
        m_impl->session = std::make_unique<Ort::Session>(
            m_impl->env, modelPath.c_str(), m_impl->sessionOptions);
        return true;
    } catch (const Ort::Exception &ex) {
        if (errorOut) {
            *errorOut = ex.what();
        }
        m_impl->session.reset();
        return false;
    } catch (const std::exception &ex) {
        if (errorOut) {
            *errorOut = ex.what();
        }
        m_impl->session.reset();
        return false;
    }
}

bool OnnxCwModel::run(const SpectrogramTensor &spectrogram, InferenceResult *out,
                      std::string *errorOut)
{
    if (!isLoaded() || !out) {
        if (errorOut) {
            *errorOut = "CW model is not loaded";
        }
        return false;
    }
    if (spectrogram.timeSteps <= 0 || spectrogram.data.empty()) {
        if (errorOut) {
            *errorOut = "Empty spectrogram";
        }
        return false;
    }

    try {
        const std::array<int64_t, 4> inputShape = {
            1, 1, static_cast<int64_t>(spectrogram.timeSteps),
            static_cast<int64_t>(kCroppedBins)};

        Ort::MemoryInfo memoryInfo =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float *>(spectrogram.data.data()),
            spectrogram.data.size(), inputShape.data(), inputShape.size());

        const char *inputNames[] = {"spectrogram"};
        const char *outputNames[] = {"log_probs"};

        auto outputs = m_impl->session->Run(Ort::RunOptions{nullptr}, inputNames,
                                            &inputTensor, 1, outputNames, 1);

        if (outputs.empty() || !outputs[0].IsTensor()) {
            if (errorOut) {
                *errorOut = "Missing log_probs tensor";
            }
            return false;
        }

        auto info = outputs[0].GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        // Expected [1, T_out, 42]
        if (shape.size() != 3) {
            if (errorOut) {
                *errorOut = "Unexpected log_probs rank";
            }
            return false;
        }

        const int timeSteps = static_cast<int>(shape[1]);
        const int numClasses = static_cast<int>(shape[2]);
        const float *data = outputs[0].GetTensorData<float>();
        const size_t count = static_cast<size_t>(timeSteps) * static_cast<size_t>(numClasses);

        out->timeSteps = timeSteps;
        out->numClasses = numClasses;
        out->logProbs.assign(data, data + count);
        return true;
    } catch (const Ort::Exception &ex) {
        if (errorOut) {
            *errorOut = ex.what();
        }
        return false;
    } catch (const std::exception &ex) {
        if (errorOut) {
            *errorOut = ex.what();
        }
        return false;
    }
}

} // namespace deepcw
