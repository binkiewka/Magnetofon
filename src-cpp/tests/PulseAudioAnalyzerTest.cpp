#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include "PulseAudioAnalyzer.hpp"
#include <limits>
#include <memory>
#include <algorithm>
#include <cmath>

class PulseAudioAnalyzerTest : public QObject {
    Q_OBJECT
private slots:
    void silenceAndKnownFrequencies() {
        PulseAudioAnalyzer analyzer(false);
        analyzer.m_active.store(true);
        std::array<float, PulseAudioAnalyzer::kFrames * 2> pcm{};
        std::array<double, 16> spectrum{};
        double left = -1, right = -1;
        analyzer.analyze(pcm.data(), PulseAudioAnalyzer::kFrames, PulseAudioAnalyzer::kSampleRate);
        analyzer.m_available.store(true);
        QVERIFY(analyzer.snapshot(left, right, spectrum));
        QCOMPARE(left, 0.0);
        QCOMPARE(right, 0.0);
        for (double level : spectrum) QCOMPARE(level, 0.0);

        // Exact FFT bins minimize leakage. Different channels must not be
        // confused with a whole-system mixed signal or decorative random data.
        for (int frame = 0; frame < PulseAudioAnalyzer::kFrames; ++frame) {
            pcm[frame * 2] = 0.25f * std::sin(2 * M_PI * 10 * frame / PulseAudioAnalyzer::kFrames);
            pcm[frame * 2 + 1] = 0;
        }
        analyzer.analyze(pcm.data(), PulseAudioAnalyzer::kFrames, PulseAudioAnalyzer::kSampleRate);
        QVERIFY(analyzer.snapshot(left, right, spectrum));
        QVERIFY(left > 0.7);
        QCOMPARE(right, 0.0);
        const auto peak = std::max_element(spectrum.begin(), spectrum.end()) - spectrum.begin();
        QVERIFY(peak >= 5 && peak <= 7); // 468.75 Hz, logarithmic 40–18000 Hz bands.
        analyzer.m_lastSample -= std::chrono::seconds(1);
        QVERIFY(!analyzer.snapshot(left, right, spectrum));
    }

    void pcmMailboxIsBoundedFiniteFreshAndSequenced() {
        PulseAudioAnalyzer analyzer(false);
        analyzer.m_active.store(true);
        analyzer.m_available.store(true);
        std::array<float, PulseAudioAnalyzer::kFrames * 2> pcm{}, copy{};
        std::uint64_t sequence = 0;
        QVERIFY(!analyzer.pcmSnapshot(copy, sequence));
        pcm[0] = 0.25f;
        pcm[1] = std::numeric_limits<float>::quiet_NaN();
        pcm[2] = std::numeric_limits<float>::infinity();
        analyzer.analyze(pcm.data(), PulseAudioAnalyzer::kFrames, PulseAudioAnalyzer::kSampleRate);
        QVERIFY(analyzer.pcmSnapshot(copy, sequence));
        QCOMPARE(sequence, std::uint64_t(1));
        QCOMPARE(copy[0], 0.25f); QCOMPARE(copy[1], 0.0f); QCOMPARE(copy[2], 0.0f);
        pcm.fill(0.0f);
        analyzer.analyze(pcm.data(), PulseAudioAnalyzer::kFrames, PulseAudioAnalyzer::kSampleRate);
        QVERIFY(analyzer.pcmSnapshot(copy, sequence));
        QCOMPARE(sequence, std::uint64_t(2));
        QCOMPARE(copy[0], 0.0f);
        analyzer.m_lastSample -= std::chrono::seconds(1);
        QVERIFY(!analyzer.pcmSnapshot(copy, sequence));
    }

    void nonFiniteSamplesDoNotPoisonAnalysis() {
        PulseAudioAnalyzer analyzer(false);
        analyzer.m_active.store(true);
        analyzer.m_available.store(true);
        std::array<float, PulseAudioAnalyzer::kFrames * 2> pcm{};
        pcm[0] = std::numeric_limits<float>::quiet_NaN();
        pcm[3] = std::numeric_limits<float>::infinity();
        analyzer.analyze(pcm.data(), PulseAudioAnalyzer::kFrames, PulseAudioAnalyzer::kSampleRate);
        double left, right;
        std::array<double, 16> spectrum{};
        QVERIFY(analyzer.snapshot(left, right, spectrum));
        QCOMPARE(left, 0.0);
        QCOMPARE(right, 0.0);
        for (double level : spectrum) QCOMPARE(level, 0.0);
    }

    void unavailableAndInactiveAreNotMeasurements() {
        PulseAudioAnalyzer analyzer(false);
        double left = 0, right = 0;
        std::array<double, 16> spectrum{};
        QVERIFY(!analyzer.snapshot(left, right, spectrum));
        analyzer.m_available.store(true);
        QVERIFY(!analyzer.snapshot(left, right, spectrum));
    }

    void stalledServerHandshakeDoesNotBlockShutdown() {
#ifdef MAGNETOFON_HAVE_PULSE
        QTemporaryDir temp;
        QLocalServer server;
        const QString endpoint = temp.filePath("pulse");
        QVERIFY(server.listen(endpoint));
        const QByteArray previous = qgetenv("PULSE_SERVER");
        const bool hadServer = qEnvironmentVariableIsSet("PULSE_SERVER");
        qputenv("PULSE_SERVER", ("unix:" + endpoint).toUtf8());
        auto analyzer = std::make_unique<PulseAudioAnalyzer>();
        analyzer->setActive(true);
        // This socket accepts but never speaks PulseAudio. No host sound server
        // is contacted, and shutdown must not await a protocol reply.
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
        std::unique_ptr<QLocalSocket> socket(server.nextPendingConnection());
        QElapsedTimer elapsed;
        elapsed.start();
        analyzer.reset();
        const auto shutdownMs = elapsed.elapsed();
        if (hadServer) qputenv("PULSE_SERVER", previous); else qunsetenv("PULSE_SERVER");
        QVERIFY2(shutdownMs < 500, qPrintable(QString("shutdown took %1 ms").arg(shutdownMs)));
#else
        PulseAudioAnalyzer analyzer(false); // Non-Pulse builds remain safely unavailable.
        QVERIFY(!analyzer.available());
#endif
    }
};

QTEST_GUILESS_MAIN(PulseAudioAnalyzerTest)
#include "PulseAudioAnalyzerTest.moc"
