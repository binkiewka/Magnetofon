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
class VideoWindow;

class AudioPlayer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool hasLoadedMedia READ hasLoadedMedia NOTIFY hasLoadedMediaChanged)
    Q_PROPERTY(QString surroundMode READ surroundMode WRITE setSurroundMode NOTIFY surroundModeChanged)
    Q_PROPERTY(int sourceChannels READ sourceChannels NOTIFY sourceChannelsChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY audioTracksChanged)
    Q_PROPERTY(int selectedAudioTrackId READ selectedAudioTrackId NOTIFY audioTracksChanged)
    Q_PROPERTY(QString sourceAudioLabel READ sourceAudioLabel NOTIFY audioTracksChanged)
    Q_PROPERTY(int outputChannels READ outputChannels NOTIFY outputAudioChanged)
    Q_PROPERTY(int outputSampleRate READ outputSampleRate NOTIFY outputAudioChanged)
    Q_PROPERTY(QString outputChannelLayout READ outputChannelLayout NOTIFY outputAudioChanged)
    Q_PROPERTY(QString outputSampleFormat READ outputSampleFormat NOTIFY outputAudioChanged)
    Q_PROPERTY(QString decodedAudioLabel READ decodedAudioLabel NOTIFY outputAudioChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY videoInfoChanged)
    Q_PROPERTY(bool videoVisible READ videoVisible NOTIFY videoVisibleChanged)
    Q_PROPERTY(QString videoCodec READ videoCodec NOTIFY videoInfoChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoInfoChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoInfoChanged)
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
    int sourceChannels() const { return m_sourceChannels; }
    QVariantList audioTracks() const { return m_audioTracks; }
    int selectedAudioTrackId() const { return m_selectedAudioTrackId; }
    QString sourceAudioLabel() const;
    int outputChannels() const { return m_outputChannels; }
    int outputSampleRate() const { return m_outputSampleRate; }
    QString outputChannelLayout() const { return m_outputChannelLayout; }
    QString outputSampleFormat() const { return m_outputSampleFormat; }
    QString decodedAudioLabel() const;
    bool hasVideo() const { return m_hasVideo; }
    bool videoVisible() const { return m_videoVisible; }
    QString videoCodec() const { return m_videoCodec; }
    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }
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
    void selectAudioTrack(int trackId);
    void showVideo();
    void hideVideo();
    void toggleVideo();

signals:
    void currentFileChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void isPlayingChanged();
    void hasLoadedMediaChanged();
    void surroundModeChanged();
    void sourceChannelsChanged();
    void audioTracksChanged();
    void outputAudioChanged();
    void videoInfoChanged();
    void videoVisibleChanged();
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
    void updateSourceAudioParams();
    void updateAudioTracks();
    void updateOutputAudioParams();
    void updateVideoInfo();
    void clearMediaInfo();

    mpv_handle *m_mpv = nullptr;
    QTimer *m_eventTimer = nullptr;
    QTimer *m_analysisTimer = nullptr;
    std::unique_ptr<PulseAudioAnalyzer> m_audioAnalyzer;
    std::unique_ptr<VideoWindow> m_videoWindow;

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
    int m_sourceChannels = 0;
    QVariantList m_audioTracks;
    int m_selectedAudioTrackId = -1;
    int m_outputChannels = 0;
    int m_outputSampleRate = 0;
    QString m_outputChannelLayout;
    QString m_outputSampleFormat;
    bool m_hasVideo = false;
    bool m_videoVisible = false;
    QString m_videoCodec;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
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
