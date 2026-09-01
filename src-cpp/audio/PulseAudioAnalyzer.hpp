#ifndef PULSE_AUDIO_ANALYZER_HPP
#define PULSE_AUDIO_ANALYZER_HPP

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class PulseAudioAnalyzer {
public:
    PulseAudioAnalyzer();
    ~PulseAudioAnalyzer();

    PulseAudioAnalyzer(const PulseAudioAnalyzer &) = delete;
    PulseAudioAnalyzer &operator=(const PulseAudioAnalyzer &) = delete;

    bool snapshot(double &left, double &right, std::array<double, 16> &spectrum) const;
    bool available() const { return m_available.load(); }

private:
    void captureLoop();
    void analyze(const float *interleaved, int frames, int sampleRate);
    static std::string defaultMonitorSource();

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_available{false};
    std::thread m_thread;

    mutable std::mutex m_mutex;
    double m_left = 0.0;
    double m_right = 0.0;
    std::array<double, 16> m_spectrum{};
};

#endif // PULSE_AUDIO_ANALYZER_HPP
