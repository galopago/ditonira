#pragma once

#include <QString>

namespace deepcw {

/// Resolve deepcw_en.onnx (wide) next to the binary / build tree / resources.
QString findDeepCwModelPath();

/// Resolve deepcw_en_narrow.onnx (pileup) next to the binary / build tree / resources.
QString findDeepCwNarrowModelPath();

} // namespace deepcw
