#include "PulseAudioAnalyzer.hpp"

#include <QProcess>
#include <QThread>
#include <pulse/error.h>
#include <pulse/simple.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace {
constexpr int kSampleRate = 48000;
constexpr int kFrames = 1024;
constexpr int kChannels = 2;

double normalizedLevel(double rms)
{
    const double db = 20.0 * std::log10(std::max(1.0e-7, rms));
    return std::clamp((db + 55.0) / 50.0, 0.0, 1.0);
}

void fft(std::vector<std::complex<double>> &values)
{
    const int n = static_cast<int>(values.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * M_PI / len;
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                const auto even = values[i + j];
                const auto odd = values[i + j + len / 2] * w;
                values[i + j] = even + odd;
                values[i + j + len / 2] = even - odd;
                w *= step;
            }
        }
    }
}
} // namespace

PulseAudioAnalyzer::PulseAudioAnalyzer()
    : m_thread(&PulseAudioAnalyzer::captureLoop, this)
{
}

PulseAudioAnalyzer::~PulseAudioAnalyzer()
{
    m_stop.store(true);
    if (m_thread.joinable()) m_thread.join();
}

bool PulseAudioAnalyzer::snapshot(double &left, double &right,
                                  std::array<double, 16> &spectrum) const
{
    if (!m_available.load()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    left = m_left;
    right = m_right;
    spectrum = m_spectrum;
    return true;
}

std::string PulseAudioAnalyzer::defaultMonitorSource()
{
    QProcess pactl;
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("get-default-sink")});
    if (!pactl.waitForStarted(1000) || !pactl.waitForFinished(1500)) return {};
    const QByteArray sink = pactl.readAllStandardOutput().trimmed();
    if (sink.isEmpty()) return {};
    return (sink + QByteArrayLiteral(".monitor")).toStdString();
}

void PulseAudioAnalyzer::captureLoop()
{
    const pa_sample_spec sampleSpec{PA_SAMPLE_FLOAT32NE, kSampleRate, kChannels};
    pa_buffer_attr bufferAttr{};
    bufferAttr.maxlength = static_cast<uint32_t>(-1);
    bufferAttr.tlength = static_cast<uint32_t>(-1);
    bufferAttr.prebuf = static_cast<uint32_t>(-1);
    bufferAttr.minreq = static_cast<uint32_t>(-1);
    bufferAttr.fragsize = kFrames * kChannels * sizeof(float);

    std::array<float, kFrames * kChannels> samples{};

    while (!m_stop.load()) {
        const std::string source = defaultMonitorSource();
        if (source.empty()) {
            for (int i = 0; i < 10 && !m_stop.load(); ++i) QThread::msleep(100);
            continue;
        }

        int error = 0;
        pa_simple *stream = pa_simple_new(nullptr, "Magnetofon", PA_STREAM_RECORD,
                                          source.c_str(), "Music visual analyzer",
                                          &sampleSpec, nullptr, &bufferAttr, &error);
        if (!stream) {
            m_available.store(false);
            for (int i = 0; i < 10 && !m_stop.load(); ++i) QThread::msleep(100);
            continue;
        }

        m_available.store(true);
        while (!m_stop.load()) {
            if (pa_simple_read(stream, samples.data(), sizeof(samples), &error) < 0) break;
            analyze(samples.data(), kFrames, kSampleRate);
        }

        m_available.store(false);
        pa_simple_free(stream);
    }
}

void PulseAudioAnalyzer::analyze(const float *interleaved, int frames, int sampleRate)
{
    double leftSquares = 0.0;
    double rightSquares = 0.0;
    double monoMean = 0.0;

    for (int i = 0; i < frames; ++i) {
        const double left = std::isfinite(interleaved[i * 2]) ? interleaved[i * 2] : 0.0;
        const double right = std::isfinite(interleaved[i * 2 + 1]) ? interleaved[i * 2 + 1] : 0.0;
        leftSquares += left * left;
        rightSquares += right * right;
        monoMean += (left + right) * 0.5;
    }
    monoMean /= frames;

    std::vector<std::complex<double>> bins(frames);
    for (int i = 0; i < frames; ++i) {
        const double mono = ((interleaved[i * 2] + interleaved[i * 2 + 1]) * 0.5) - monoMean;
        const double window = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (frames - 1));
        bins[i] = std::complex<double>(mono * window, 0.0);
    }
    fft(bins);

    std::array<double, 16> spectrum{};
    constexpr double minFrequency = 40.0;
    constexpr double maxFrequency = 18000.0;
    for (int band = 0; band < 16; ++band) {
        const double low = minFrequency * std::pow(maxFrequency / minFrequency, band / 16.0);
        const double high = minFrequency * std::pow(maxFrequency / minFrequency, (band + 1) / 16.0);
        const int lowBin = std::clamp(static_cast<int>(std::floor(low * frames / sampleRate)), 1, frames / 2 - 1);
        const int highBin = std::clamp(static_cast<int>(std::ceil(high * frames / sampleRate)), lowBin + 1, frames / 2);

        double magnitude = 0.0;
        for (int bin = lowBin; bin < highBin; ++bin) {
            magnitude = std::max(magnitude, std::abs(bins[bin]) * 2.0 / frames);
        }
        const double db = 20.0 * std::log10(std::max(1.0e-8, magnitude));
        spectrum[band] = std::clamp((db + 78.0) / 68.0, 0.0, 1.0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_left = normalizedLevel(std::sqrt(leftSquares / frames));
    m_right = normalizedLevel(std::sqrt(rightSquares / frames));
    m_spectrum = spectrum;
}
