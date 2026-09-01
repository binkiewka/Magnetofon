#include "AudioPlayer.hpp"
#include "PulseAudioAnalyzer.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <random>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    m_eqBands = QVariantList{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) m_spectrum.append(0.0);

    initMpv();
    m_audioAnalyzer = std::make_unique<PulseAudioAnalyzer>();

    m_eventTimer = new QTimer(this);
    connect(m_eventTimer, &QTimer::timeout, this, &AudioPlayer::processMpvEvents);
    m_eventTimer->start(20);

    m_analysisTimer = new QTimer(this);
    connect(m_analysisTimer, &QTimer::timeout, this, &AudioPlayer::updateAudioAnalysis);
    m_analysisTimer->start(33);
}

AudioPlayer::~AudioPlayer()
{
    m_audioAnalyzer.reset();
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void AudioPlayer::initMpv()
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        qCritical() << "[AudioPlayer] Failed to create mpv instance";
        return;
    }

    mpv_set_option_string(m_mpv, "no-config", "yes");
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "ao", "pulse,pipewire,alsa");
    mpv_set_option_string(m_mpv, "audio-client-name", "Magnetofon");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "audio-channels", "auto-safe");
    mpv_set_option_string(m_mpv, "gapless-audio", "no");

    const int status = mpv_initialize(m_mpv);
    if (status < 0) {
        qCritical() << "[AudioPlayer] Failed to initialize mpv:" << mpv_error_string(status);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
}

void AudioPlayer::setFileLoaded(bool loaded)
{
    if (m_fileLoaded == loaded) return;
    m_fileLoaded = loaded;
    emit hasLoadedMediaChanged();
    if (!loaded) setPlaying(false);
}

void AudioPlayer::setPlaying(bool playing)
{
    playing = playing && m_fileLoaded && !m_currentFile.isEmpty();
    if (m_isPlaying == playing) return;
    m_isPlaying = playing;
    emit isPlayingChanged();
}

void AudioPlayer::queueLoadCurrentFile()
{
    if (!m_mpv || m_currentFile.isEmpty() || m_loadPending) return;
    const QByteArray encodedPath = QFile::encodeName(m_currentFile);
    const char *command[] = {"loadfile", encodedPath.constData(), "replace", nullptr};
    const int status = mpv_command_async(m_mpv, 0, command);
    if (status < 0) {
        qWarning() << "[AudioPlayer] Failed to queue file:" << mpv_error_string(status);
        return;
    }
    m_loadPending = true;
}

void AudioPlayer::processMpvEvents()
{
    if (!m_mpv) return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
        case MPV_EVENT_START_FILE:
            setFileLoaded(false);
            break;

        case MPV_EVENT_FILE_LOADED: {
            m_loadPending = false;
            setFileLoaded(true);
            if (m_playWhenLoaded) {
                int paused = 0;
                m_paused = false;
                mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
            }
            setPlaying(!m_paused);
            applyAudioFilters();
            break;
        }

        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *property = static_cast<mpv_event_property *>(event->data);
            if (strcmp(property->name, "time-pos") == 0 && property->format == MPV_FORMAT_DOUBLE) {
                if (property->data) {
                    const double value = *static_cast<double *>(property->data);
                    if (std::abs(m_position - value) > 0.03) {
                        m_position = value;
                        emit positionChanged();
                    }
                }
            } else if (strcmp(property->name, "duration") == 0 && property->format == MPV_FORMAT_DOUBLE) {
                if (property->data) {
                    const double value = *static_cast<double *>(property->data);
                    if (std::abs(m_duration - value) > 0.03) {
                        m_duration = value;
                        emit durationChanged();
                    }
                }
            } else if (strcmp(property->name, "pause") == 0 && property->format == MPV_FORMAT_FLAG) {
                if (property->data) {
                    m_paused = *static_cast<int *>(property->data) != 0;
                    setPlaying(!m_paused);
                }
            }
            break;
        }

        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<mpv_event_end_file *>(event->data);
            const bool reachedEnd = end && end->reason == MPV_END_FILE_REASON_EOF;
            if (end && end->reason == MPV_END_FILE_REASON_ERROR) {
                qWarning() << "[AudioPlayer] Playback failed:" << mpv_error_string(end->error);
            }
            m_loadPending = false;
            m_playWhenLoaded = false;
            setFileLoaded(false);
            resetAnalysis();
            if (reachedEnd) emit trackEnded();
            break;
        }

        default:
            break;
        }
    }
}

void AudioPlayer::load(const QString &filePath)
{
    if (!m_mpv) return;

    const QFileInfo file(filePath);
    if (!file.exists() || !file.isFile()) {
        qWarning() << "[AudioPlayer] File does not exist:" << filePath;
        return;
    }

    if (m_currentFile != file.absoluteFilePath()) {
        m_currentFile = file.absoluteFilePath();
        emit currentFileChanged();
    }

    m_playWhenLoaded = false;
    m_loadPending = false;
    setFileLoaded(false);
    setPlaying(false);

    if (m_position != 0.0) {
        m_position = 0.0;
        emit positionChanged();
    }
    if (m_duration != 0.0) {
        m_duration = 0.0;
        emit durationChanged();
    }

    int paused = 1;
    m_paused = true;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    queueLoadCurrentFile();
    setVolume(m_volume);
}

void AudioPlayer::play()
{
    if (!m_mpv || m_currentFile.isEmpty()) return;
    m_playWhenLoaded = true;
    if (!m_fileLoaded) queueLoadCurrentFile();

    int paused = 0;
    m_paused = false;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    setPlaying(true);
}

void AudioPlayer::pause()
{
    if (!m_mpv) return;
    m_playWhenLoaded = false;
    int paused = 1;
    m_paused = true;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &paused);
    setPlaying(false);
}

void AudioPlayer::togglePlayPause()
{
    if (m_isPlaying) pause();
    else play();
}

void AudioPlayer::stop()
{
    if (!m_mpv) return;
    m_playWhenLoaded = false;
    m_loadPending = false;
    const char *command[] = {"stop", nullptr};
    mpv_command_async(m_mpv, 0, command);
    setFileLoaded(false);
    setPlaying(false);
    if (m_position != 0.0) {
        m_position = 0.0;
        emit positionChanged();
    }
    resetAnalysis();
}

void AudioPlayer::seek(double seconds)
{
    if (!m_mpv || m_currentFile.isEmpty()) return;
    const QByteArray encodedValue = QByteArray::number(std::max(0.0, seconds), 'f', 3);
    const char *command[] = {"seek", encodedValue.constData(), "absolute", nullptr};
    mpv_command_async(m_mpv, 0, command);
}

void AudioPlayer::setVolume(double volume)
{
    const double clamped = std::clamp(volume, 0.0, 1.0);
    if (std::abs(m_volume - clamped) > 0.0001) {
        m_volume = clamped;
        emit volumeChanged();
    }
    if (!m_mpv) return;
    double mpvVolume = m_volume * 100.0;
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &mpvVolume);
}

void AudioPlayer::setSurroundMode(const QString &mode)
{
    if (m_surroundMode == mode) return;
    m_surroundMode = mode;
    emit surroundModeChanged();
    applyAudioFilters();
}

void AudioPlayer::setEqEnabled(bool enabled)
{
    if (m_eqEnabled == enabled) return;
    m_eqEnabled = enabled;
    emit eqEnabledChanged();
    applyAudioFilters();
}

void AudioPlayer::setPreamp(double preamp)
{
    if (std::abs(m_preamp - preamp) < 0.01) return;
    m_preamp = preamp;
    emit preampChanged();
    applyAudioFilters();
}

void AudioPlayer::setEqBands(const QVariantList &bands)
{
    if (m_eqBands == bands) return;
    m_eqBands = bands;
    emit eqBandsChanged();
    applyAudioFilters();
}

void AudioPlayer::applyAudioFilters()
{
    if (!m_mpv) return;

    QStringList filters;
    if (m_eqEnabled) {
        if (std::abs(m_preamp) > 0.01) {
            filters.append(QString("volume=%1dB").arg(m_preamp, 0, 'f', 2));
        }

        static const int frequencies[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
        for (int i = 0; i < std::min(10, static_cast<int>(m_eqBands.size())); ++i) {
            const double gain = m_eqBands[i].toDouble();
            if (std::abs(gain) > 0.01) {
                filters.append(QString("equalizer=f=%1:width_type=o:width=1:g=%2")
                                   .arg(frequencies[i])
                                   .arg(gain, 0, 'f', 2));
            }
        }
    }

    if (m_surroundMode.compare("SURROUND", Qt::CaseInsensitive) == 0) {
        filters.append("pan=5.1|FL=FL|FR=FR|FC=0.55*FL+0.55*FR|LFE=0.25*FL+0.25*FR|BL=0.45*FR|BR=0.45*FL");
    }

    const QString filter = filters.isEmpty() ? QString() : QString("lavfi=[%1]").arg(filters.join(','));
    const QByteArray encodedFilter = filter.toUtf8();
    const int status = mpv_set_property_string(m_mpv, "af", encodedFilter.constData());
    if (status < 0) qWarning() << "[AudioPlayer] Failed to apply filters:" << mpv_error_string(status);
}

void AudioPlayer::resetAnalysis(bool immediate)
{
    if (immediate) {
        m_leftMeter = 0.0;
        m_rightMeter = 0.0;
        for (int i = 0; i < m_spectrum.size(); ++i) m_spectrum[i] = 0.0;
        emit metersChanged();
        emit spectrumChanged();
    }
}

void AudioPlayer::updateAudioAnalysis()
{
    if (!m_isPlaying || !m_fileLoaded) {
        bool metersChanged = false;
        if (m_leftMeter > 0.0005 || m_rightMeter > 0.0005) {
            m_leftMeter *= 0.72;
            m_rightMeter *= 0.72;
            if (m_leftMeter < 0.001) m_leftMeter = 0.0;
            if (m_rightMeter < 0.001) m_rightMeter = 0.0;
            metersChanged = true;
        }

        bool spectrumChanged = false;
        for (int i = 0; i < m_spectrum.size(); ++i) {
            double value = m_spectrum[i].toDouble();
            if (value > 0.0005) {
                value *= 0.68;
                if (value < 0.001) value = 0.0;
                m_spectrum[i] = value;
                spectrumChanged = true;
            }
        }
        if (metersChanged) emit this->metersChanged();
        if (spectrumChanged) emit this->spectrumChanged();
        return;
    }

    double targetLeft = 0.0;
    double targetRight = 0.0;
    std::array<double, 16> targetSpectrum{};
    const bool hasRealAudio = m_audioAnalyzer &&
                              m_audioAnalyzer->snapshot(targetLeft, targetRight, targetSpectrum);

    if (!hasRealAudio) {
        static std::mt19937 random(1337);
        static std::uniform_real_distribution<double> energy(0.42, 0.92);
        const double base = energy(random) * m_volume;
        targetLeft = base * (0.86 + 0.14 * std::sin(m_position * 4.2));
        targetRight = base * (0.86 + 0.14 * std::cos(m_position * 4.5));
        for (int i = 0; i < 16; ++i) {
            const double weighting = 1.0 - i / 21.0;
            targetSpectrum[i] = base * weighting *
                                (0.5 + 0.5 * std::sin(m_position * 8.0 + i * 0.42));
        }
    }

    const double leftRate = targetLeft > m_leftMeter ? 0.55 : 0.18;
    const double rightRate = targetRight > m_rightMeter ? 0.55 : 0.18;
    m_leftMeter += (targetLeft - m_leftMeter) * leftRate;
    m_rightMeter += (targetRight - m_rightMeter) * rightRate;

    for (int i = 0; i < 16; ++i) {
        const double current = m_spectrum[i].toDouble();
        const double rate = targetSpectrum[i] > current ? 0.52 : 0.2;
        m_spectrum[i] = current + (targetSpectrum[i] - current) * rate;
    }

    emit metersChanged();
    emit spectrumChanged();
}
