#ifndef VISUALIZER_LAUNCHER_HPP
#define VISUALIZER_LAUNCHER_HPP

#include <QObject>
#include <QProcess>
#include <QString>

class VisualizerLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY settingsChanged)
    Q_PROPERTY(int presetDuration READ presetDuration WRITE setPresetDuration NOTIFY settingsChanged)
    Q_PROPERTY(double transitionDuration READ transitionDuration WRITE setTransitionDuration NOTIFY settingsChanged)
    Q_PROPERTY(bool hardCutsEnabled READ hardCutsEnabled WRITE setHardCutsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double hardCutSensitivity READ hardCutSensitivity WRITE setHardCutSensitivity NOTIFY settingsChanged)
    Q_PROPERTY(int hardCutDuration READ hardCutDuration WRITE setHardCutDuration NOTIFY settingsChanged)
    Q_PROPERTY(QString presetSource READ presetSource WRITE setPresetSource NOTIFY settingsChanged)

public:
    explicit VisualizerLauncher(QObject *parent = nullptr);
    ~VisualizerLauncher() override;

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

    int targetFps() const { return m_targetFps; }
    void setTargetFps(int fps) { if (m_targetFps != fps) { m_targetFps = fps; emit settingsChanged(); restartIfRunning(); } }

    int presetDuration() const { return m_presetDuration; }
    void setPresetDuration(int duration) { if (m_presetDuration != duration) { m_presetDuration = duration; emit settingsChanged(); restartIfRunning(); } }

    double transitionDuration() const { return m_transitionDuration; }
    void setTransitionDuration(double duration) { if (m_transitionDuration != duration) { m_transitionDuration = duration; emit settingsChanged(); restartIfRunning(); } }

    bool hardCutsEnabled() const { return m_hardCutsEnabled; }
    void setHardCutsEnabled(bool enabled) { if (m_hardCutsEnabled != enabled) { m_hardCutsEnabled = enabled; emit settingsChanged(); restartIfRunning(); } }

    double hardCutSensitivity() const { return m_hardCutSensitivity; }
    void setHardCutSensitivity(double sens) { if (m_hardCutSensitivity != sens) { m_hardCutSensitivity = sens; emit settingsChanged(); restartIfRunning(); } }

    int hardCutDuration() const { return m_hardCutDuration; }
    void setHardCutDuration(int duration) { if (m_hardCutDuration != duration) { m_hardCutDuration = duration; emit settingsChanged(); restartIfRunning(); } }

    QString presetSource() const { return m_presetSource; }
    void setPresetSource(const QString &source) { if (m_presetSource != source) { m_presetSource = source; emit settingsChanged(); restartIfRunning(); } }

    Q_INVOKABLE void toggleVisuals();
    Q_INVOKABLE void launchVisuals();
    Q_INVOKABLE void stopVisuals();
    Q_INVOKABLE void restartIfRunning();

signals:
    void isRunningChanged();
    void settingsChanged();

private slots:
    void onProcessStateChanged(QProcess::ProcessState newState);

private:
    QString findProjectMBinary() const;
    QString findPresetDirectory() const;

    QProcess *m_process = nullptr;
    int m_targetFps = 60;
    int m_presetDuration = 15;
    double m_transitionDuration = 2.5;
    bool m_hardCutsEnabled = true;
    double m_hardCutSensitivity = 1.0;
    int m_hardCutDuration = 10;
    QString m_presetSource = "CURATED";
};

#endif // VISUALIZER_LAUNCHER_HPP
