#ifndef VISUALIZER_LAUNCHER_HPP
#define VISUALIZER_LAUNCHER_HPP

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>

class QTemporaryDir;

class VisualizerLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY settingsChanged)
    Q_PROPERTY(int presetDuration READ presetDuration WRITE setPresetDuration NOTIFY settingsChanged)
    Q_PROPERTY(double transitionDuration READ transitionDuration WRITE setTransitionDuration NOTIFY settingsChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled WRITE setShuffleEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool borderlessWindow READ borderlessWindow WRITE setBorderlessWindow NOTIFY settingsChanged)
    Q_PROPERTY(double beatSensitivity READ beatSensitivity WRITE setBeatSensitivity NOTIFY settingsChanged)
    Q_PROPERTY(bool hardCutsEnabled READ hardCutsEnabled WRITE setHardCutsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double hardCutSensitivity READ hardCutSensitivity WRITE setHardCutSensitivity NOTIFY settingsChanged)
    Q_PROPERTY(int hardCutDuration READ hardCutDuration WRITE setHardCutDuration NOTIFY settingsChanged)
    Q_PROPERTY(QString presetSource READ presetSource WRITE setPresetSource NOTIFY settingsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY runtimeInfoChanged)
    Q_PROPERTY(QString currentPresetName READ currentPresetName NOTIFY runtimeInfoChanged)
    Q_PROPERTY(QString activePresetDirectory READ activePresetDirectory NOTIFY presetLibraryChanged)
    Q_PROPERTY(QString audioSourceName READ audioSourceName NOTIFY runtimeInfoChanged)
    Q_PROPERTY(int presetCount READ presetCount NOTIFY presetLibraryChanged)
    Q_PROPERTY(int curatedPresetCount READ curatedPresetCount NOTIFY presetLibraryChanged)
    Q_PROPERTY(int fullPresetCount READ fullPresetCount NOTIFY presetLibraryChanged)
    Q_PROPERTY(bool fullLibraryAvailable READ fullLibraryAvailable NOTIFY presetLibraryChanged)

public:
    explicit VisualizerLauncher(QObject *parent = nullptr, bool restoreSettings = true);
    ~VisualizerLauncher() override;

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

    int targetFps() const { return m_targetFps; }
    void setTargetFps(int fps);

    int presetDuration() const { return m_presetDuration; }
    void setPresetDuration(int duration);

    double transitionDuration() const { return m_transitionDuration; }
    void setTransitionDuration(double duration);

    bool shuffleEnabled() const { return m_shuffleEnabled; }
    void setShuffleEnabled(bool enabled);

    bool borderlessWindow() const { return m_borderlessWindow; }
    void setBorderlessWindow(bool enabled);

    double beatSensitivity() const { return m_beatSensitivity; }
    void setBeatSensitivity(double sensitivity);

    bool hardCutsEnabled() const { return m_hardCutsEnabled; }
    void setHardCutsEnabled(bool enabled);

    double hardCutSensitivity() const { return m_hardCutSensitivity; }
    void setHardCutSensitivity(double sensitivity);

    int hardCutDuration() const { return m_hardCutDuration; }
    void setHardCutDuration(int duration);

    QString presetSource() const { return m_presetSource; }
    void setPresetSource(const QString &source);

    QString statusMessage() const { return m_statusMessage; }
    QString currentPresetName() const { return m_currentPresetName; }
    QString activePresetDirectory() const { return m_activePresetDirectory; }
    QString audioSourceName() const { return m_audioSourceName; }
    int presetCount() const { return m_presetCount; }
    int curatedPresetCount() const { return m_curatedPresetCount; }
    int fullPresetCount() const { return m_fullPresetCount; }
    bool fullLibraryAvailable() const { return m_fullPresetCount > 0; }

    QStringList buildArguments() const;

    Q_INVOKABLE void toggleVisuals();
    Q_INVOKABLE void launchVisuals();
    Q_INVOKABLE void stopVisuals();
    Q_INVOKABLE void restartIfRunning();
    Q_INVOKABLE void refreshPresetLibrary();

signals:
    void isRunningChanged();
    void settingsChanged();
    void runtimeInfoChanged();
    void presetLibraryChanged();

private slots:
    void onProcessStateChanged(QProcess::ProcessState newState);
    void onProcessError(QProcess::ProcessError error);
    void readProcessOutput();
    void performRestart();

private:
    QString findProjectMBinary() const;
    QString findPresetDirectory(const QString &source) const;
    QString findTextureDirectory() const;
    QString resolveAudioMonitorSource() const;
    static int countPresetFiles(const QString &directory);
    void loadSettings();
    void saveSetting(const QString &key, const QVariant &value) const;
    void scheduleRestartIfRunning();
    void requestProcessStop(bool restartAfterStop);
    void setStatusMessage(const QString &message);

    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_projectMConfigDirectory;
    QTimer m_restartTimer;
    QTimer m_forceKillTimer;
    QString m_processOutputBuffer;
    bool m_restartAfterStop = false;
    bool m_processFailed = false;
    bool m_persistSettings = true;

    int m_targetFps = 60;
    int m_presetDuration = 15;
    double m_transitionDuration = 2.5;
    bool m_shuffleEnabled = true;
    bool m_borderlessWindow = true;
    double m_beatSensitivity = 1.0;
    bool m_hardCutsEnabled = true;
    double m_hardCutSensitivity = 1.0;
    int m_hardCutDuration = 10;
    QString m_presetSource = QStringLiteral("CURATED");

    QString m_statusMessage = QStringLiteral("Visualizer stopped");
    QString m_currentPresetName;
    QString m_activePresetDirectory;
    QString m_audioSourceName;
    int m_presetCount = 0;
    int m_curatedPresetCount = 0;
    int m_fullPresetCount = 0;
};

#endif // VISUALIZER_LAUNCHER_HPP
