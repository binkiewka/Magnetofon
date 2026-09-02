#include "VisualizerLauncher.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>
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
    const QString pwd = QDir::currentPath();
    const QString appDir = QCoreApplication::applicationDirPath();

    const QStringList candidatePresetDirs = {
        appData + "/visuals",
        pwd + "/visuals",
        pwd + "/resources/projectm/linux-x64/presets",
        appDir + "/resources/projectm/linux-x64/presets",
        appDir + "/../resources/projectm/linux-x64/presets",
        appDir + "/../../resources/projectm/linux-x64/presets"
    };

    for (const QString &dirPath : candidatePresetDirs) {
        QDir dir(dirPath);
        if (dir.exists()) {
            qDebug() << "[VisualizerLauncher] Found preset directory at:" << dirPath;
            return dirPath;
        }
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
    const QFileInfo binInfo(binary);
    const QString binDir = binInfo.absolutePath();
    const QString libDir = QFileInfo(binDir + "/../lib").absoluteFilePath();

    qDebug() << "[VisualizerLauncher] Launching visualizer:" << binary << "with presets:" << presets << "libDir:" << libDir;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (QDir(libDir).exists()) {
        const QString existingLd = env.value("LD_LIBRARY_PATH");
        env.insert("LD_LIBRARY_PATH", libDir + (existingLd.isEmpty() ? "" : ":" + existingLd));
    }
    m_process->setProcessEnvironment(env);
    if (binInfo.exists()) {
        m_process->setWorkingDirectory(binDir);
    }

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
