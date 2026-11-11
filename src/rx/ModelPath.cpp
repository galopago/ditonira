#include "rx/ModelPath.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace deepcw {
namespace {

QString findModelFile(const QString &fileName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/../share/ditonira/models/") + fileName,
        appDir + QStringLiteral("/share/ditonira/models/") + fileName,
        appDir + QStringLiteral("/../../resources/models/") + fileName,
        appDir + QStringLiteral("/../resources/models/") + fileName,
        QStringLiteral("resources/models/") + fileName,
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return candidates.front();
}

} // namespace

QString findDeepCwModelPath()
{
    return findModelFile(QStringLiteral("deepcw_en.onnx"));
}

QString findDeepCwNarrowModelPath()
{
    return findModelFile(QStringLiteral("deepcw_en_narrow.onnx"));
}

} // namespace deepcw
