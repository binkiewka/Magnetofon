#include "AudioRouting.hpp"

namespace AudioRouting {

QString outputChannels(const QString &mode)
{
    return mode.compare(QStringLiteral("STEREO"), Qt::CaseInsensitive) == 0
               ? QStringLiteral("stereo")
               : QStringLiteral("auto-safe");
}

bool shouldUpmixToSurround(const QString &mode, int sourceChannels)
{
    return mode.compare(QStringLiteral("SURROUND"), Qt::CaseInsensitive) == 0
           && sourceChannels > 0 && sourceChannels <= 2;
}

QString surroundUpmixFilter()
{
    return QStringLiteral(
        "pan=5.1|FL=FL|FR=FR|FC=0.55*FL+0.55*FR|LFE=0.25*FL+0.25*FR|BL=0.45*FR|BR=0.45*FL");
}

} // namespace AudioRouting
