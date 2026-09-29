#include "AudioRouting.hpp"
#include <QStringList>

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

QString bassManagedUpmixFilter(bool surround, int speakerHz, int subHz, double subDb)
{
    // Normalize mono to stereo, then derive LFE independently from the full-range source.
    // A 5.1 carrier keeps FL/FR/LFE correctly mapped on common surround devices.
    const QString highpass = speakerHz > 0 ? QString("highpass=f=%1:p=2,").arg(speakerHz) : QString();
    const QString pan = surround
        ? QStringLiteral("pan=5.1|FL=FL|FR=FR|FC=0.5*FL+0.5*FR|LFE=0*FL|BL=0.45*FR|BR=0.45*FL")
        : QStringLiteral("pan=5.1|FL=FL|FR=FR|FC=0*FL|LFE=0*FL|BL=0*FL|BR=0*FR");
    return QString("aformat=channel_layouts=stereo,asplit=2[main][bass];"
                   "[main]%1%2[speakers];"
                   "[bass]pan=mono|c0=0.5*FL+0.5*FR,lowpass=f=%3:p=2,volume=%4dB,"
                   "pan=5.1|FL=0*c0|FR=0*c0|FC=0*c0|LFE=c0|BL=0*c0|BR=0*c0[sub];"
                   "[speakers][sub]amix=inputs=2:normalize=0")
        .arg(highpass, pan).arg(subHz).arg(subDb, 0, 'f', 2);
}

QString surroundBassManagementFilter(const std::array<int, 5> &cutoffs, int subHz,
                                     double subDb, bool upmix, bool sideLayout)
{
    QStringList graph;
    // Keep native channel order, including side vs back 5.1. Never downmix a native mix.
    QString input;
    if (upmix) {
        input = "aformat=channel_layouts=stereo,"
                "pan=5.1|FL=FL|FR=FR|FC=0.5*FL+0.5*FR|LFE=0*FL|BL=0.45*FR|BR=0.45*FL,";
        sideLayout = false;
    }
    graph << input + "asplit=6[in0][in1][in2][in3][in4][in5]";
    QStringList bassInputs;
    bassInputs << "[nativeLfe]";
    graph << "[in3]pan=mono|c0=c3[nativeLfe]";
    const int channels[] = {0, 1, 2, 4, 5};
    for (int i = 0; i < 5; ++i) {
        const int channel = channels[i];
        const QString source = QString("[in%1]pan=mono|c0=c%1").arg(channel);
        if (cutoffs[i] > 0) {
            graph << source + QString(",asplit=2[high%1][low%1]").arg(channel);
            graph << QString("[high%1]highpass=f=%2:p=2[out%1]").arg(channel).arg(cutoffs[i]);
            graph << QString("[low%1]lowpass=f=%2:p=2[bass%1]").arg(channel).arg(cutoffs[i]);
            bassInputs << QString("[bass%1]").arg(channel);
        } else {
            graph << source + QString("[out%1]").arg(channel);
        }
    }
    // Redirect removed bass into the original LFE, then apply the independent sub profile.
    graph << bassInputs.join("") + QString("amix=inputs=%1:normalize=0,lowpass=f=%2:p=2,volume=%3dB[out3]")
        .arg(bassInputs.size()).arg(subHz).arg(subDb, 0, 'f', 2);
    const QString layout = sideLayout ? "5.1(side)" : "5.1";
    const QString left = sideLayout ? "SL" : "BL";
    const QString right = sideLayout ? "SR" : "BR";
    graph << QString("[out0][out1][out2][out3][out4][out5]join=inputs=6:channel_layout=%1:"
                     "map=0.0-FL|1.0-FR|2.0-FC|3.0-LFE|4.0-%2|5.0-%3").arg(layout, left, right);
    return graph.join(';');
}

} // namespace AudioRouting
