#include "PulseAudioAnalyzer.hpp"

#ifdef MAGNETOFON_HAVE_PULSE
#include <pulse/pulseaudio.h>
#include <unistd.h>
#include <cstring>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>

namespace {

double normalizedLevel(double rms)
{
    const double db = 20.0 * std::log10(std::max(1.0e-7, rms));
    return std::clamp((db + 55.0) / 50.0, 0.0, 1.0);
}

template <size_t N>
void fft(std::array<std::complex<double>, N> &values)
{
    constexpr int n = static_cast<int>(N);
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

PulseAudioAnalyzer::PulseAudioAnalyzer(bool captureEnabled, long targetProcessId, int captureSampleRate)
    : m_captureEnabled(captureEnabled)
    , m_targetProcessId(targetProcessId)
    , m_captureSampleRate(std::clamp(captureSampleRate, 8000, 192000))
{
    for (int i = 0; i < kFrames; ++i) {
        m_window[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (kFrames - 1));
    }
    m_thread = std::thread(&PulseAudioAnalyzer::captureLoop, this);
}

PulseAudioAnalyzer::~PulseAudioAnalyzer()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop.store(true, std::memory_order_release);
        m_active.store(false, std::memory_order_release);
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void PulseAudioAnalyzer::setActive(bool active)
{
    const bool previous = m_active.exchange(active, std::memory_order_acq_rel);
    if (previous != active) {
        m_cv.notify_all();
    }
}

bool PulseAudioAnalyzer::snapshot(double &left, double &right,
                                  std::array<double, 16> &spectrum) const
{
    if (!m_available.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active.load(std::memory_order_acquire)
        || std::chrono::steady_clock::now() - m_lastSample > std::chrono::milliseconds(500)) return false;
    left = m_left;
    right = m_right;
    spectrum = m_spectrum;
    return true;
}

bool PulseAudioAnalyzer::pcmSnapshot(std::array<float, kFrames * kChannels> &samples,
                                      std::uint64_t &sequence) const
{
    if (!m_available.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active.load(std::memory_order_acquire) || !m_pcmSequence
        || std::chrono::steady_clock::now() - m_lastSample > std::chrono::milliseconds(500)) return false;
    samples = m_pcmSamples;
    sequence = m_pcmSequence;
    return true;
}

// All Pulse objects and callbacks belong to the worker. No blocking simple-API
// read/connect and no cross-thread stream destruction are used.
#ifdef MAGNETOFON_HAVE_PULSE
struct PulseAudioAnalyzer::CaptureSession {
    PulseAudioAnalyzer &owner;
    pa_mainloop *loop = nullptr;
    pa_context *context = nullptr;
    pa_stream *stream = nullptr;
    uint32_t input = PA_INVALID_INDEX;
    uint32_t sink = PA_INVALID_INDEX;
    bool enumerating = false;
    bool found = false;
    bool sinkPending = false;
    std::string processId;
    std::array<float, kFrames * kChannels> samples{};
    size_t filled = 0;

    explicit CaptureSession(PulseAudioAnalyzer &analyzer) : owner(analyzer),
        processId(std::to_string(analyzer.m_targetProcessId > 0 ? analyzer.m_targetProcessId : getpid())) {}
    ~CaptureSession() {
        closeStream();
        if (context) { pa_context_disconnect(context); pa_context_unref(context); }
        if (loop) pa_mainloop_free(loop);
    }
    void closeStream() {
        owner.m_available.store(false, std::memory_order_release);
        if (stream) {
            pa_stream_set_read_callback(stream, nullptr, nullptr);
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);
            stream = nullptr;
        }
        filled = 0;
    }
    static void read(pa_stream *stream, size_t, void *userdata) {
        auto &self = *static_cast<CaptureSession *>(userdata);
        const void *data = nullptr;
        size_t bytes = 0;
        if (pa_stream_peek(stream, &data, &bytes) < 0 || !bytes) return;
        // Metering only needs recent samples. Bound callback CPU even when a
        // server supplies an unexpectedly large fragment; NULL is a silent hole.
        const size_t frameBytes = sizeof(float) * kChannels;
        const size_t frames = bytes / frameBytes;
        const size_t take = std::min(frames, static_cast<size_t>(kFrames));
        if (frames >= static_cast<size_t>(kFrames)) self.filled = 0;
        const char *source = data ? static_cast<const char *>(data) + (frames - take) * frameBytes : nullptr;
        size_t offset = 0;
        while (offset < take) {
            const size_t count = std::min(take - offset, static_cast<size_t>(kFrames) - self.filled);
            float *destination = self.samples.data() + self.filled * kChannels;
            if (source) std::memcpy(destination, source + offset * frameBytes, count * frameBytes);
            else std::fill_n(destination, count * kChannels, 0.0f);
            self.filled += count;
            offset += count;
            if (self.filled == static_cast<size_t>(kFrames)) {
                self.owner.analyze(self.samples.data(), kFrames, self.owner.m_captureSampleRate);
                self.owner.m_available.store(true, std::memory_order_release);
                self.filled = 0;
            }
        }
        if (pa_stream_drop(stream) < 0) self.owner.m_available.store(false, std::memory_order_release);
    }
    static void sinkInfo(pa_context *context, const pa_sink_info *info, int eol, void *userdata) {
        auto &self = *static_cast<CaptureSession *>(userdata);
        if (eol) { self.sinkPending = false; return; }
        if (!info || info->index != self.sink || !info->monitor_source_name
            || self.owner.m_stop.load() || !self.owner.m_active.load() || self.stream) return;
        const pa_sample_spec spec{PA_SAMPLE_FLOAT32NE, static_cast<uint32_t>(self.owner.m_captureSampleRate), kChannels};
        self.stream = pa_stream_new(context, "Magnetofon isolated signal meter", &spec, nullptr);
        if (!self.stream) return;
        // A monitor stream restricted to this sink-input never measures browsers,
        // system notifications, or another player's mixed output.
        if (pa_stream_set_monitor_stream(self.stream, self.input) < 0) { self.closeStream(); return; }
        pa_stream_set_read_callback(self.stream, &CaptureSession::read, &self);
        pa_buffer_attr attr{};
        attr.maxlength = static_cast<uint32_t>(-1);
        attr.tlength = attr.prebuf = attr.minreq = static_cast<uint32_t>(-1);
        attr.fragsize = kFrames * kChannels * sizeof(float);
        if (pa_stream_connect_record(self.stream, info->monitor_source_name, &attr,
                PA_STREAM_ADJUST_LATENCY) < 0) self.closeStream();
    }
    static void inputInfo(pa_context *context, const pa_sink_input_info *info, int eol, void *userdata) {
        auto &self = *static_cast<CaptureSession *>(userdata);
        if (eol) {
            self.enumerating = false;
            if (!self.found) { self.closeStream(); self.input = self.sink = PA_INVALID_INDEX; }
            return;
        }
        if (!info || !info->proplist) return;
        const char *pid = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_ID);
        if (!pid || self.processId != pid || self.found) return;
        self.found = true;
        if (self.input != info->index || self.sink != info->sink) {
            self.closeStream();
            self.input = info->index;
            self.sink = info->sink;
        }
        if (!self.stream && !self.sinkPending) {
            self.sinkPending = true;
            if (auto *op = pa_context_get_sink_info_by_index(context, self.sink, &CaptureSession::sinkInfo, &self))
                pa_operation_unref(op);
            else self.sinkPending = false;
        }
    }
};
#endif

void PulseAudioAnalyzer::captureLoop()
{
#ifdef MAGNETOFON_HAVE_PULSE
    while (!m_stop.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop.load() || (m_captureEnabled && m_active.load()); });
        }
        if (m_stop.load()) break;
        {
            CaptureSession session(*this);
            session.loop = pa_mainloop_new();
            if (session.loop) session.context = pa_context_new(pa_mainloop_get_api(session.loop), "Magnetofon signal meter");
            if (session.context && pa_context_connect(session.context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) >= 0) {
                auto nextQuery = std::chrono::steady_clock::now();
                const auto connectDeadline = nextQuery + std::chrono::seconds(3);
                while (!m_stop.load() && m_active.load()) {
                    int result = 0;
                    if (pa_mainloop_iterate(session.loop, 0, &result) < 0) break;
                    const auto state = pa_context_get_state(session.context);
                    if (!PA_CONTEXT_IS_GOOD(state)) break;
                    const auto now = std::chrono::steady_clock::now();
                    if (state != PA_CONTEXT_READY && now > connectDeadline) break;
                    if (session.stream && !PA_STREAM_IS_GOOD(pa_stream_get_state(session.stream))) session.closeStream();
                    if (state == PA_CONTEXT_READY && now >= nextQuery && !session.enumerating) {
                        session.enumerating = true;
                        session.found = false;
                        if (auto *op = pa_context_get_sink_input_info_list(session.context, &CaptureSession::inputInfo, &session))
                            pa_operation_unref(op);
                        else session.enumerating = false;
                        nextQuery = now + std::chrono::milliseconds(500);
                    }
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait_for(lock, std::chrono::milliseconds(10), [this] { return m_stop.load() || !m_active.load(); });
                }
            }
        } // Context/stream/mainloop cleaned up on their worker before joining.
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(500), [this] { return m_stop.load() || !m_active.load(); });
    }
#else
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_stop.load(); });
#endif
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

    for (int i = 0; i < frames; ++i) {
        const double left = std::isfinite(interleaved[i * 2]) ? interleaved[i * 2] : 0.0;
        const double right = std::isfinite(interleaved[i * 2 + 1]) ? interleaved[i * 2 + 1] : 0.0;
        const double mono = ((left + right) * 0.5) - monoMean;
        m_fftBins[i] = std::complex<double>(mono * m_window[i], 0.0);
    }
    fft(m_fftBins);

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
            magnitude = std::max(magnitude, std::abs(m_fftBins[bin]) * 2.0 / frames);
        }
        const double db = 20.0 * std::log10(std::max(1.0e-8, magnitude));
        spectrum[band] = std::clamp((db + 78.0) / 68.0, 0.0, 1.0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // A bounded latest-block mailbox shared with the native visualizer. Capture
    // is restricted to the selected process before connection, not system mix.
    m_pcmSamples.fill(0.0f);
    const int sampleCount = std::min(frames, kFrames) * kChannels;
    for (int index = 0; index < sampleCount; ++index)
        m_pcmSamples[index] = std::isfinite(interleaved[index])
            ? std::clamp(interleaved[index], -4.0f, 4.0f) : 0.0f;
    ++m_pcmSequence;
    m_left = normalizedLevel(std::sqrt(leftSquares / frames));
    m_right = normalizedLevel(std::sqrt(rightSquares / frames));
    m_spectrum = spectrum;
    m_lastSample = std::chrono::steady_clock::now();
}
