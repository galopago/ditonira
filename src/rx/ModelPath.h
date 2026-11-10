#pragma once

#include <QString>

namespace deepcw {

/// Resolve deepcw_en.onnx next to the binary / build tree / resources.
QString findDeepCwModelPath();

} // namespace deepcw
