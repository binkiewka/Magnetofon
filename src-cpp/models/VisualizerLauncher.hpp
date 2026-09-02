#ifndef VISUALIZER_LAUNCHER_HPP
#define VISUALIZER_LAUNCHER_HPP

#include <QObject>
#include <QProcess>
#include <QString>

class VisualizerLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)

public:
    explicit VisualizerLauncher(QObject *parent = nullptr);
    ~VisualizerLauncher() override;

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

    Q_INVOKABLE void toggleVisuals();
    Q_INVOKABLE void launchVisuals();
    Q_INVOKABLE void stopVisuals();

signals:
    void isRunningChanged();

private slots:
    void onProcessStateChanged(QProcess::ProcessState newState);

private:
    QString findProjectMBinary() const;
    QString findPresetDirectory() const;

    QProcess *m_process = nullptr;
};

#endif // VISUALIZER_LAUNCHER_HPP
