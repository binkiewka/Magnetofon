#ifndef PULSE_AUDIO_ANALYZER_HPP
#define PULSE_AUDIO_ANALYZER_HPP

#include <array>
#include <atomic>
#include <complex>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class PulseAudioAnalyzer {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kFrames = 1024;
    static constexpr int kChannels = 2;

    explicit PulseAudioAnalyzer(bool captureEnabled = true, long targetProcessId = 0,
                                int captureSampleRate = kSampleRate);
    ~PulseAudioAnalyzer();

    PulseAudioAnalyzer(const PulseAudioAnalyzer &) = delete;
    PulseAudioAnalyzer &operator=(const PulseAudioAnalyzer &) = delete;

    void setActive(bool active);
    bool snapshot(double &left, double &right, std::array<double, 16> &spectrum) const;
    bool pcmSnapshot(std::array<float, kFrames * kChannels> &samples, std::uint64_t &sequence) const;
    bool available() const { return m_available.load(std::memory_order_acquire); }

private:
    friend class PulseAudioAnalyzerTest;
    struct CaptureSession;
    void captureLoop();
    void analyze(const float *interleaved, int frames, int sampleRate);

    const bool m_captureEnabled;
    const long m_targetProcessId;
    const int m_captureSampleRate;
    std::chrono::steady_clock::time_point m_lastSample{};
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_available{false};
    std::thread m_thread;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::array<float, kFrames * kChannels> m_pcmSamples{};
    std::uint64_t m_pcmSequence = 0;
    double m_left = 0.0;
    double m_right = 0.0;
    std::array<double, 16> m_spectrum{};
    std::array<std::complex<double>, kFrames> m_fftBins{};
    std::array<double, kFrames> m_window{};
};

#endif // PULSE_AUDIO_ANALYZER_HPP
