#include "rx/ModelPath.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace deepcw {

QString findDeepCwModelPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/../share/ditonira/models/deepcw_en.onnx"),
        appDir + QStringLiteral("/share/ditonira/models/deepcw_en.onnx"),
        appDir + QStringLiteral("/../../resources/models/deepcw_en.onnx"),
        appDir + QStringLiteral("/../resources/models/deepcw_en.onnx"),
        QStringLiteral("resources/models/deepcw_en.onnx"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return candidates.front();
}

} // namespace deepcw
