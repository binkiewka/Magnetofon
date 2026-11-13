#include <QtTest>
#include "PulseAudioAnalyzer.hpp"
#include <pulse/pulseaudio.h>
#include <unistd.h>
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

// This executable replaces the complete libpulse boundary used by the original
// analyzer source. No fake function reaches a host server, sound device or user
// configuration. Worker callbacks and stream cleanup still run in production code.
struct pa_context {};
struct pa_mainloop {};
struct pa_operation {};
struct pa_proplist { bool own; };
struct pa_stream {
    pa_stream_request_cb_t read = nullptr;
    void *userdata = nullptr;
    uint32_t monitor = PA_INVALID_INDEX;
};

namespace {
pa_mainloop_api api{};
pa_stream *currentStream = nullptr; // Worker-owned, like the original API handles.
const std::string ownPid = std::to_string(getpid());
std::string streamProcessId = ownPid;
std::atomic<int> capturedSampleRate{0};
std::atomic<bool> includeOwn{true}, deliver{true};
std::atomic<int> sampleMode{0}, sinkIndex{3}, connectedSink{-1};
std::atomic<int> streamCreates{0}, streamCloses{0}, monitoredInput{-1}, blockingIterations{0};
std::array<float, PulseAudioAnalyzer::kFrames * 2> fragment{};
void reset() {
    includeOwn = true; deliver = true; sampleMode = 0; sinkIndex = 3;
    streamProcessId = ownPid; capturedSampleRate = 0;
    connectedSink = -1; streamCreates = 0; streamCloses = 0;
    monitoredInput = -1; blockingIterations = 0;
}
}

extern "C" {
pa_mainloop *pa_mainloop_new() { return new pa_mainloop; }
void pa_mainloop_free(pa_mainloop *loop) { delete loop; }
pa_mainloop_api *pa_mainloop_get_api(pa_mainloop *) { return &api; }
int pa_mainloop_iterate(pa_mainloop *, int block, int *) {
    if (block) ++blockingIterations;
    if (currentStream && currentStream->read && deliver.load()) {
        currentStream->read(currentStream, fragment.size() * sizeof(float), currentStream->userdata);
    }
    return 0;
}
pa_context *pa_context_new(pa_mainloop_api *, const char *) { return new pa_context; }
int pa_context_connect(pa_context *, const char *, pa_context_flags_t, const pa_spawn_api *) { return 0; }
pa_context_state_t pa_context_get_state(const pa_context *) { return PA_CONTEXT_READY; }
void pa_context_disconnect(pa_context *) {}
void pa_context_unref(pa_context *context) { delete context; }
void pa_operation_unref(pa_operation *operation) { delete operation; }
const char *pa_proplist_gets(const pa_proplist *properties, const char *key) {
    if (std::strcmp(key, PA_PROP_APPLICATION_PROCESS_ID)) return nullptr;
    return properties->own ? streamProcessId.c_str() : "not-this-process";
}
pa_operation *pa_context_get_sink_input_info_list(pa_context *context, pa_sink_input_info_cb_t cb, void *userdata) {
    pa_proplist other{false}, own{true};
    pa_sink_input_info input{};
    input.index = 11; input.sink = 1; input.proplist = &other;
    cb(context, &input, 0, userdata); // A different app comes first, deliberately.
    if (includeOwn.load()) {
        input.index = 22; input.sink = static_cast<uint32_t>(sinkIndex.load()); input.proplist = &own;
        cb(context, &input, 0, userdata);
    }
    cb(context, nullptr, 1, userdata);
    return new pa_operation;
}
pa_operation *pa_context_get_sink_info_by_index(pa_context *context, uint32_t index, pa_sink_info_cb_t cb, void *userdata) {
    pa_sink_info info{};
    const std::string monitor = "monitor-" + std::to_string(index);
    info.index = index; info.monitor_source_name = monitor.c_str();
    cb(context, &info, 0, userdata);
    cb(context, nullptr, 1, userdata);
    return new pa_operation;
}
pa_stream *pa_stream_new(pa_context *, const char *, const pa_sample_spec *spec, const pa_channel_map *) {
    capturedSampleRate = spec->rate;
    ++streamCreates;
    currentStream = new pa_stream;
    return currentStream;
}
int pa_stream_set_monitor_stream(pa_stream *stream, uint32_t input) {
    stream->monitor = input;
    monitoredInput = static_cast<int>(input);
    return 0;
}
void pa_stream_set_read_callback(pa_stream *stream, pa_stream_request_cb_t cb, void *userdata) {
    stream->read = cb; stream->userdata = userdata;
}
int pa_stream_connect_record(pa_stream *stream, const char *source, const pa_buffer_attr *, pa_stream_flags_t) {
    // Restriction must precede connecting to any monitor. Missing restriction
    // would let a whole-system recording accidentally satisfy a positive-level test.
    if (stream->monitor != 22) return -1;
    connectedSink = std::stoi(std::string(source).substr(8));
    return 0;
}
pa_stream_state_t pa_stream_get_state(const pa_stream *) { return PA_STREAM_READY; }
int pa_stream_disconnect(pa_stream *) { ++streamCloses; return 0; }
void pa_stream_unref(pa_stream *stream) {
    if (currentStream == stream) currentStream = nullptr;
    delete stream;
}
int pa_stream_peek(pa_stream *, const void **data, size_t *bytes) {
    const int mode = sampleMode.load();
    for (int i = 0; i < PulseAudioAnalyzer::kFrames; ++i) {
        fragment[i * 2] = mode == 1 ? 0.25f * std::sin(2 * M_PI * 10 * i / PulseAudioAnalyzer::kFrames) : 0;
        fragment[i * 2 + 1] = 0;
    }
    *data = mode == 2 ? nullptr : fragment.data(); // A silent PulseAudio hole.
    *bytes = fragment.size() * sizeof(float);
    return 0;
}
int pa_stream_drop(pa_stream *) { return 0; }
}

class PulseAudioIsolationTest : public QObject {
    Q_OBJECT
private slots:
    void init() { reset(); }

    void ownInputIsRestrictedAndSilenceIsNotOtherAppsAudio() {
        PulseAudioAnalyzer analyzer;
        analyzer.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(analyzer.available(), 1000);
        QCOMPARE(monitoredInput.load(), 22);
        QCOMPARE(connectedSink.load(), 3);
        QCOMPARE(blockingIterations.load(), 0);
        double left = -1, right = -1;
        std::array<double, 16> spectrum{};
        QVERIFY(analyzer.snapshot(left, right, spectrum));
        QCOMPARE(left, 0.0); QCOMPARE(right, 0.0);
        sampleMode = 1;
        QTRY_VERIFY_WITH_TIMEOUT(analyzer.snapshot(left, right, spectrum) && left > 0.7, 1000);
        QCOMPARE(right, 0.0);
        sampleMode = 2;
        QTRY_VERIFY_WITH_TIMEOUT(analyzer.snapshot(left, right, spectrum) && left == 0.0, 1000);
    }

    void explicitParentPidAndVisualizerRateRemainProcessRestricted() {
        streamProcessId = "424242";
        PulseAudioAnalyzer analyzer(true, 424242, 44100);
        analyzer.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(analyzer.available(), 1000);
        QCOMPARE(monitoredInput.load(), 22);
        QCOMPARE(capturedSampleRate.load(), 44100);
        std::array<float, PulseAudioAnalyzer::kFrames * PulseAudioAnalyzer::kChannels> pcm{};
        std::uint64_t sequence = 0;
        QVERIFY(analyzer.pcmSnapshot(pcm, sequence));
        QVERIFY(sequence > 0);
        QCOMPARE(pcm[0], 0.0f);
    }

    void noOwnInputNeverFallsBackToMixedMonitor() {
        includeOwn = false;
        sampleMode = 1;
        PulseAudioAnalyzer analyzer;
        analyzer.setActive(true);
        QTest::qWait(650);
        QVERIFY(!analyzer.available());
        QCOMPARE(streamCreates.load(), 0);
        QCOMPARE(monitoredInput.load(), -1);
    }

    void movedInputReopensAndRemovedInputInvalidates() {
        PulseAudioAnalyzer analyzer;
        analyzer.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(analyzer.available(), 1000);
        sinkIndex = 7;
        QTRY_COMPARE_WITH_TIMEOUT(connectedSink.load(), 7, 1500);
        QVERIFY(streamCreates.load() >= 2);
        QVERIFY(streamCloses.load() >= 1);
        QCOMPARE(monitoredInput.load(), 22);
        includeOwn = false;
        QTRY_VERIFY_WITH_TIMEOUT(!analyzer.available(), 1500);
        double left, right; std::array<double, 16> spectrum{};
        QVERIFY(!analyzer.snapshot(left, right, spectrum));
    }

    void noDataBecomesUnavailableAndShutdownDoesNotAwaitRead() {
        auto analyzer = std::make_unique<PulseAudioAnalyzer>();
        analyzer->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(analyzer->available(), 1000);
        deliver = false;
        QTest::qWait(550);
        double left, right; std::array<double, 16> spectrum{};
        QVERIFY(!analyzer->snapshot(left, right, spectrum));
        QElapsedTimer elapsed; elapsed.start();
        analyzer.reset();
        QVERIFY(elapsed.elapsed() < 250);
        QCOMPARE(streamCreates.load(), streamCloses.load());
        QCOMPARE(blockingIterations.load(), 0);
    }
};

QTEST_GUILESS_MAIN(PulseAudioIsolationTest)
#include "PulseAudioIsolationTest.moc"
