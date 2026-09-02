#include "VisualizerLauncher.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

VisualizerLauncher::VisualizerLauncher(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::stateChanged, this, &VisualizerLauncher::onProcessStateChanged);
}

VisualizerLauncher::~VisualizerLauncher()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
        }
    }
}

QString VisualizerLauncher::findProjectMBinary() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString pwd = QDir::currentPath();

    const QStringList candidatePaths = {
        appDir + "/resources/projectm/linux-x64/bin/projectMSDL",
        pwd + "/resources/projectm/linux-x64/bin/projectMSDL",
        appDir + "/../resources/projectm/linux-x64/bin/projectMSDL",
        appDir + "/../../resources/projectm/linux-x64/bin/projectMSDL",
        appDir + "/projectMSDL",
        appDir + "/projectMSDL.exe"
    };

    for (const QString &path : candidatePaths) {
        if (QFileInfo::exists(path)) {
            qDebug() << "[VisualizerLauncher] Found bundled projectM binary at:" << path;
            return path;
        }
    }

    return "projectMSDL";
}

QString VisualizerLauncher::findPresetDirectory() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString customVisuals = appData + "/visuals";
    if (QDir(customVisuals).exists()) {
        return customVisuals;
    }

    const QString localVisuals = QDir::currentPath() + "/visuals";
    if (QDir(localVisuals).exists()) {
        return localVisuals;
    }

    const QString pwd = QDir::currentPath();
    const QString bundledPresets = pwd + "/resources/projectm/linux-x64/presets";
    if (QDir(bundledPresets).exists()) {
        return bundledPresets;
    }

    return QString();
}

void VisualizerLauncher::toggleVisuals()
{
    if (isRunning()) {
        stopVisuals();
    } else {
        launchVisuals();
    }
}

void VisualizerLauncher::launchVisuals()
{
    if (isRunning()) return;

    const QString binary = findProjectMBinary();
    const QString presets = findPresetDirectory();

    qDebug() << "[VisualizerLauncher] Launching visualizer:" << binary << "with presets:" << presets;

    QStringList args;
    if (!presets.isEmpty()) {
        args << "-p" << presets;
    }

    m_process->start(binary, args);
    emit isRunningChanged();
}

void VisualizerLauncher::stopVisuals()
{
    if (!isRunning()) return;

    m_process->terminate();
    if (!m_process->waitForFinished(1000)) {
        m_process->kill();
    }
    emit isRunningChanged();
}

void VisualizerLauncher::onProcessStateChanged(QProcess::ProcessState newState)
{
    Q_UNUSED(newState);
    emit isRunningChanged();
}
