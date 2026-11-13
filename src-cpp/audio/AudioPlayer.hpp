#ifndef AUDIO_PLAYER_HPP
#define AUDIO_PLAYER_HPP

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QTimer>
#include <array>
#include <memory>
#include <vector>
#include <mpv/client.h>

class PlaylistModel;
class PulseAudioAnalyzer;
class VideoWindow;

class AudioPlayer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString playbackError READ playbackError NOTIFY playbackErrorChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(double volumeDb READ volumeDb NOTIFY volumeChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool protectionEnabled READ protectionEnabled WRITE setProtectionEnabled NOTIFY protectionEnabledChanged)
    Q_PROPERTY(bool gaplessEnabled READ gaplessEnabled WRITE setGaplessEnabled NOTIFY gaplessEnabledChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QVariantList surroundCutoffs READ surroundCutoffs NOTIFY speakerSettingsChanged)
    Q_PROPERTY(int surroundSubwooferCutoff READ surroundSubwooferCutoff WRITE setSurroundSubwooferCutoff NOTIFY speakerSettingsChanged)
    Q_PROPERTY(double surroundSubwooferGain READ surroundSubwooferGain WRITE setSurroundSubwooferGain NOTIFY speakerSettingsChanged)
    Q_PROPERTY(int speakerCutoff READ speakerCutoff WRITE setSpeakerCutoff NOTIFY speakerSettingsChanged)
    Q_PROPERTY(int subwooferCutoff READ subwooferCutoff WRITE setSubwooferCutoff NOTIFY speakerSettingsChanged)
    Q_PROPERTY(double subwooferGain READ subwooferGain WRITE setSubwooferGain NOTIFY speakerSettingsChanged)
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
    Q_PROPERTY(bool analysisAvailable READ analysisAvailable NOTIFY analysisAvailableChanged)
    Q_PROPERTY(double leftMeter READ leftMeter NOTIFY metersChanged)
    Q_PROPERTY(double rightMeter READ rightMeter NOTIFY metersChanged)
    Q_PROPERTY(QVariantList spectrum READ spectrum NOTIFY spectrumChanged)

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    void setPlaylist(PlaylistModel *playlist);
    QString playbackError() const { return m_playbackError; }
    bool loading() const { return m_loadPending; }
    double volumeDb() const;
    QString repeatMode() const { return m_repeatMode; }
    bool protectionEnabled() const { return m_protectionEnabled; }
    bool gaplessEnabled() const { return m_gaplessEnabled; }
    QString currentFile() const { return m_currentFile; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    QVariantList surroundCutoffs() const;
    int surroundSubwooferCutoff() const { return m_surroundSubwooferCutoff; }
    double surroundSubwooferGain() const { return m_surroundSubwooferGain; }
    int speakerCutoff() const { return m_speakerCutoff; }
    int subwooferCutoff() const { return m_subwooferCutoff; }
    double subwooferGain() const { return m_subwooferGain; }
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

    bool analysisAvailable() const { return m_analysisAvailable; }
    double leftMeter() const { return m_leftMeter; }
    double rightMeter() const { return m_rightMeter; }
    QVariantList spectrum() const { return m_spectrum; }

public slots:
    void setGaplessEnabled(bool enabled);
    bool load(const QString &filePath);
    bool restorePaused(const QString &filePath, double positionSeconds);
    void retry();
    void next();
    void previous();
    void setRepeatMode(const QString &mode);
    void setProtectionEnabled(bool enabled);
    void resetSoundProfile();
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void seek(double seconds);
    void setSurroundCutoff(int speaker, int hz);
    void setSurroundSubwooferCutoff(int hz);
    void setSurroundSubwooferGain(double db);
    void resetSpeakerProfile(bool surround);
    void setSpeakerCutoff(int hz);
    void setSubwooferCutoff(int hz);
    void setSubwooferGain(double db);
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
    void playbackErrorChanged();
    void loadingChanged();
    void repeatModeChanged();
    void protectionEnabledChanged();
    void gaplessEnabledChanged();
    void currentFileChanged();
    void positionChanged();
    void durationChanged();
    void speakerSettingsChanged();
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
    void analysisAvailableChanged();
    void metersChanged();
    void spectrumChanged();
    void trackEnded();

private slots:
    void processMpvEvents();
    void updateAudioAnalysis();

private:
    void unload();
    void setLoading(bool loading);
    void setPlaybackError(const QString &error);
    void advanceAfterEof();
    void persistSoundProfile();
    void setAnalysisAvailable(bool available);
    void syncNextTrack();
    void rememberNativeQueue();
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

    QPointer<PlaylistModel> m_playlist;
    QTimer m_queueSyncTimer;
    bool m_gaplessEnabled = false;
    QString m_repeatMode = QStringLiteral("OFF");
    bool m_protectionEnabled = true;
    QString m_playbackError;
    QString m_failedFile;
    double m_restorePosition = -1.0;
    QStringList m_queuedFiles;
    QHash<qint64, QString> m_nativeFiles;
    qint64 m_eventEntryId = -1;
    QString m_startedFile;
    QString m_appliedFilter;
    QByteArray m_appliedChannels;
    QString m_currentFile;
    double m_position = 0.0;
    double m_duration = 0.0;
    std::array<int, 5> m_surroundCutoffs{{80, 80, 80, 80, 80}};
    int m_surroundSubwooferCutoff = 120;
    double m_surroundSubwooferGain = 0.0;
    QString m_sourceChannelLayout;
    int m_speakerCutoff = 80;
    int m_subwooferCutoff = 80;
    double m_subwooferGain = 0.0;
    double m_volume = 0.5;
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

    bool m_analysisAvailable = false;
    double m_leftMeter = 0.0;
    double m_rightMeter = 0.0;
    QVariantList m_spectrum;

};

#endif // AUDIO_PLAYER_HPP
