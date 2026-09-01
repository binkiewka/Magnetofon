#ifndef AUDIO_PLAYER_HPP
#define AUDIO_PLAYER_HPP

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTimer>
#include <array>
#include <memory>
#include <vector>
#include <mpv/client.h>

class PulseAudioAnalyzer;

class AudioPlayer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool hasLoadedMedia READ hasLoadedMedia NOTIFY hasLoadedMediaChanged)
    Q_PROPERTY(QString surroundMode READ surroundMode WRITE setSurroundMode NOTIFY surroundModeChanged)
    Q_PROPERTY(bool eqEnabled READ eqEnabled WRITE setEqEnabled NOTIFY eqEnabledChanged)
    Q_PROPERTY(double preamp READ preamp WRITE setPreamp NOTIFY preampChanged)
    Q_PROPERTY(QVariantList eqBands READ eqBands WRITE setEqBands NOTIFY eqBandsChanged)
    Q_PROPERTY(double leftMeter READ leftMeter NOTIFY metersChanged)
    Q_PROPERTY(double rightMeter READ rightMeter NOTIFY metersChanged)
    Q_PROPERTY(QVariantList spectrum READ spectrum NOTIFY spectrumChanged)

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    QString currentFile() const { return m_currentFile; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double volume() const { return m_volume; }
    bool isPlaying() const { return m_isPlaying; }
    bool hasLoadedMedia() const { return m_fileLoaded; }
    QString surroundMode() const { return m_surroundMode; }
    bool eqEnabled() const { return m_eqEnabled; }
    double preamp() const { return m_preamp; }
    QVariantList eqBands() const { return m_eqBands; }

    double leftMeter() const { return m_leftMeter; }
    double rightMeter() const { return m_rightMeter; }
    QVariantList spectrum() const { return m_spectrum; }

public slots:
    void load(const QString &filePath);
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void seek(double seconds);
    void setVolume(double volume);
    void setSurroundMode(const QString &mode);
    void setEqEnabled(bool enabled);
    void setPreamp(double preamp);
    void setEqBands(const QVariantList &bands);

signals:
    void currentFileChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void isPlayingChanged();
    void hasLoadedMediaChanged();
    void surroundModeChanged();
    void eqEnabledChanged();
    void preampChanged();
    void eqBandsChanged();
    void metersChanged();
    void spectrumChanged();
    void trackEnded();

private slots:
    void processMpvEvents();
    void updateAudioAnalysis();

private:
    void applyAudioFilters();
    void initMpv();
    void queueLoadCurrentFile();
    void setFileLoaded(bool loaded);
    void setPlaying(bool playing);
    void resetAnalysis(bool immediate = false);

    mpv_handle *m_mpv = nullptr;
    QTimer *m_eventTimer = nullptr;
    QTimer *m_analysisTimer = nullptr;
    std::unique_ptr<PulseAudioAnalyzer> m_audioAnalyzer;

    QString m_currentFile;
    double m_position = 0.0;
    double m_duration = 0.0;
    double m_volume = 0.8;
    bool m_isPlaying = false;
    bool m_fileLoaded = false;
    bool m_loadPending = false;
    bool m_paused = false;
    bool m_playWhenLoaded = false;
    QString m_surroundMode = "AUTO";
    bool m_eqEnabled = false;
    double m_preamp = -3.0;
    QVariantList m_eqBands;

    double m_leftMeter = 0.0;
    double m_rightMeter = 0.0;
    QVariantList m_spectrum;

    // Simulated/calculated peak meter states for smooth UI
    double m_targetLeft = 0.0;
    double m_targetRight = 0.0;
};

#endif // AUDIO_PLAYER_HPP
