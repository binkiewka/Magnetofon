#ifndef AUDIO_ROUTING_HPP
#define AUDIO_ROUTING_HPP

#include <QString>
#include <array>

namespace AudioRouting {

// AUTO preserves the source layout when the output supports it. STEREO asks
// mpv for a proper downmix. SURROUND bass-manages 5.1 without downmixing,
// and synthesizes 5.1 when the source is mono or stereo.
QString outputChannels(const QString &mode);
bool shouldUpmixToSurround(const QString &mode, int sourceChannels);
QString surroundUpmixFilter();
QString bassManagedUpmixFilter(bool surround, int speakerHz, int subHz, double subDb);
// Cutoff order: front left, front right, center, surround left, surround right.
QString surroundBassManagementFilter(const std::array<int, 5> &cutoffs, int subHz,
                                     double subDb, bool upmix, bool sideLayout);

} // namespace AudioRouting

#endif // AUDIO_ROUTING_HPP
