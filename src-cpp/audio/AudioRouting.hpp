#ifndef AUDIO_ROUTING_HPP
#define AUDIO_ROUTING_HPP

#include <QString>

namespace AudioRouting {

// AUTO preserves the source layout when the output supports it. STEREO asks
// mpv for a proper downmix. SURROUND preserves native multichannel sources and
// only synthesizes 5.1 when the source is mono or stereo.
QString outputChannels(const QString &mode);
bool shouldUpmixToSurround(const QString &mode, int sourceChannels);
QString surroundUpmixFilter();

} // namespace AudioRouting

#endif // AUDIO_ROUTING_HPP
