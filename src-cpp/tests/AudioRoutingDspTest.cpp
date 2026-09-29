#include <QtTest>
#include "AudioRouting.hpp"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
}
#include <array>
#include <cmath>

class AudioRoutingDspTest : public QObject {
    Q_OBJECT
    struct Result { std::array<double, 6> rms{}; QString layout; bool ok = false; };
    Result render(const QString &source, const QString &filter)
    {
        Result result;
        AVFilterGraph *graph = avfilter_graph_alloc();
        AVFilterContext *sink = nullptr;
        AVFilterInOut *inputs = avfilter_inout_alloc();
        AVFrame *frame = av_frame_alloc();
        auto cleanup = [&]() {
            av_frame_free(&frame);
            avfilter_inout_free(&inputs);
            avfilter_graph_free(&graph);
        };
        if (avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr, graph) < 0) {
            cleanup(); return result;
        }
        inputs->name = av_strdup("out"); inputs->filter_ctx = sink; inputs->pad_idx = 0;
        const QByteArray description = (source + "," + filter + ",aformat=sample_fmts=fltp[out]").toUtf8();
        if (avfilter_graph_parse_ptr(graph, description.constData(), &inputs, nullptr, nullptr) < 0
            || avfilter_graph_config(graph, nullptr) < 0) { cleanup(); return result; }
        long samples = 0;
        int status;
        while ((status = av_buffersink_get_frame(sink, frame)) >= 0) {
            if (frame->ch_layout.nb_channels != 6) { cleanup(); return result; }
            char layout[80]{};
            av_channel_layout_describe(&frame->ch_layout, layout, sizeof(layout));
            result.layout = QString::fromLatin1(layout);
            // Ignore filter startup transients.
            if (frame->pts > 4800) {
                for (int c = 0; c < 6; ++c) {
                    const float *data = reinterpret_cast<const float *>(frame->extended_data[c]);
                    for (int i = 0; i < frame->nb_samples; ++i) result.rms[c] += data[i] * data[i];
                }
                samples += frame->nb_samples;
            }
            av_frame_unref(frame);
        }
        result.ok = samples > 0 && status == AVERROR_EOF;
        if (samples) for (auto &rms : result.rms) rms = std::sqrt(rms / samples);
        cleanup();
        return result;
    }
    QString source(int channel, int frequency, bool side)
    {
        QStringList expressions;
        for (int c = 0; c < 6; ++c)
            expressions << (c == channel ? QString("0.1*sin(2*PI*%1*t)").arg(frequency) : "0");
        return QString("aevalsrc=exprs=%1:s=48000:d=0.5:c=%2")
            .arg(expressions.join('|'), side ? "5.1(side)" : "5.1");
    }
private slots:
    void individualCrossoversAndChannelMapping()
    {
        const int channels[] = {0, 1, 2, 4, 5};
        for (bool side : {false, true}) {
            for (int i = 0; i < 5; ++i) {
                std::array<int, 5> cutoffs{};
                cutoffs[i] = 160;
                const QString filter = AudioRouting::surroundBassManagementFilter(cutoffs, 200, 0, false, side);
                const auto low = render(source(channels[i], 40, side), filter);
                QVERIFY(low.ok);
                QCOMPARE(low.layout, side ? QString("5.1(side)") : QString("5.1"));
                QVERIFY(low.rms[3] > low.rms[channels[i]] * 10);
                for (int c : channels) if (c != channels[i]) QVERIFY(low.rms[c] < 1e-7);
                const auto high = render(source(channels[i], 1000, side), filter);
                QVERIFY(high.ok);
                QVERIFY(high.rms[channels[i]] > high.rms[3] * 100);
                // A different speaker at full range must not use this cutoff.
                const int other = channels[(i + 1) % 5];
                const auto unaffected = render(source(other, 40, side), filter);
                QVERIFY(unaffected.ok);
                QVERIFY(unaffected.rms[other] > 0.06);
                QVERIFY(unaffected.rms[3] < 1e-7);
            }
        }
    }
    void originalLfeAndIndependentLevel()
    {
        for (bool side : {false, true}) {
            const auto unity = render(source(3, 40, side), AudioRouting::surroundBassManagementFilter({}, 120, 0, false, side));
            const auto attenuated = render(source(3, 40, side), AudioRouting::surroundBassManagementFilter({}, 120, -6, false, side));
            QVERIFY(unity.ok && attenuated.ok);
            QVERIFY(unity.rms[3] > 0.06);
            QVERIFY(std::abs(attenuated.rms[3] / unity.rms[3] - std::pow(10, -6.0 / 20)) < 0.001);
            for (int c : {0, 1, 2, 4, 5}) QVERIFY(unity.rms[c] < 1e-7);
        }
    }
    void stereoUpmixAndTwoPointOne()
    {
        const QString tone = "sine=frequency=40:sample_rate=48000:duration=0.5";
        const auto surround = render(tone, AudioRouting::surroundBassManagementFilter({80, 90, 100, 110, 120}, 120, 0, true, false));
        QVERIFY(surround.ok);
        for (double rms : surround.rms) QVERIFY(rms > 0);
        const auto stereo = render(tone, AudioRouting::bassManagedUpmixFilter(false, 80, 80, 0));
        QVERIFY(stereo.ok);
        QVERIFY(stereo.rms[3] > stereo.rms[0] * 3);
        for (int c : {2, 4, 5}) QCOMPARE(stereo.rms[c], 0.0);
    }
};
QTEST_GUILESS_MAIN(AudioRoutingDspTest)
#include "AudioRoutingDspTest.moc"
