#include "VisualizerLauncher.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>
#include <QDebug>

#include <algorithm>

namespace {

QString normalizedSource(const QString &source)
{
    return source.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("ALL")
        : QStringLiteral("CURATED");
}

QString cleanExistingDirectory(const QString &path)
{
    const QDir directory(path);
    if (!directory.exists()) return {};
    const QString canonical = directory.canonicalPath();
    return canonical.isEmpty() ? directory.absolutePath() : canonical;
}

} // namespace

VisualizerLauncher::VisualizerLauncher(QObject *parent, bool restoreSettings)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_persistSettings(restoreSettings)
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::stateChanged, this, &VisualizerLauncher::onProcessStateChanged);
    connect(m_process, &QProcess::errorOccurred, this, &VisualizerLauncher::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &VisualizerLauncher::readProcessOutput);

    m_restartTimer.setSingleShot(true);
    m_restartTimer.setInterval(350);
    connect(&m_restartTimer, &QTimer::timeout, this, &VisualizerLauncher::performRestart);

    m_forceKillTimer.setSingleShot(true);
    m_forceKillTimer.setInterval(1200);
    connect(&m_forceKillTimer, &QTimer::timeout, this, [this]() {
        if (m_process->state() != QProcess::NotRunning) m_process->kill();
    });

    if (restoreSettings) loadSettings();
    refreshPresetLibrary();

    if (m_presetSource == QStringLiteral("ALL") && !fullLibraryAvailable()) {
        m_presetSource = QStringLiteral("CURATED");
        saveSetting(QStringLiteral("presetSource"), m_presetSource);
        refreshPresetLibrary();
    }
}

VisualizerLauncher::~VisualizerLauncher()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
            m_process->waitForFinished(500);
        }
    }
}

void VisualizerLauncher::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("visuals"));

    m_targetFps = std::clamp(settings.value(QStringLiteral("targetFps"), m_targetFps).toInt(), 0, 240);
    m_presetDuration = std::clamp(settings.value(QStringLiteral("presetDuration"), m_presetDuration).toInt(), 3, 240);
    m_transitionDuration = std::clamp(settings.value(QStringLiteral("transitionDuration"), m_transitionDuration).toDouble(),
                                      0.0, std::min(30.0, m_presetDuration - 0.5));
    m_shuffleEnabled = settings.value(QStringLiteral("shuffleEnabled"), m_shuffleEnabled).toBool();
    m_beatSensitivity = std::clamp(settings.value(QStringLiteral("beatSensitivity"), m_beatSensitivity).toDouble(), 0.0, 2.0);
    m_hardCutsEnabled = settings.value(QStringLiteral("hardCutsEnabled"), m_hardCutsEnabled).toBool();
    m_hardCutSensitivity = std::clamp(settings.value(QStringLiteral("hardCutSensitivity"), m_hardCutSensitivity).toDouble(), 0.0, 5.0);
    m_hardCutDuration = std::clamp(settings.value(QStringLiteral("hardCutDuration"), m_hardCutDuration).toInt(),
                                   2, std::max(2, m_presetDuration - 1));
    m_presetSource = normalizedSource(settings.value(QStringLiteral("presetSource"), m_presetSource).toString());
}

void VisualizerLauncher::saveSetting(const QString &key, const QVariant &value) const
{
    if (!m_persistSettings) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("visuals"));
    settings.setValue(key, value);
}

void VisualizerLauncher::setTargetFps(int fps)
{
    fps = std::clamp(fps, 0, 240);
    if (m_targetFps == fps) return;
    m_targetFps = fps;
    saveSetting(QStringLiteral("targetFps"), fps);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setPresetDuration(int duration)
{
    duration = std::clamp(duration, 3, 240);
    if (m_presetDuration == duration) return;

    m_presetDuration = duration;
    m_transitionDuration = std::min(m_transitionDuration, m_presetDuration - 0.5);
    m_hardCutDuration = std::min(m_hardCutDuration, std::max(2, m_presetDuration - 1));
    saveSetting(QStringLiteral("presetDuration"), m_presetDuration);
    saveSetting(QStringLiteral("transitionDuration"), m_transitionDuration);
    saveSetting(QStringLiteral("hardCutDuration"), m_hardCutDuration);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setTransitionDuration(double duration)
{
    duration = std::clamp(duration, 0.0, std::min(30.0, m_presetDuration - 0.5));
    if (qFuzzyCompare(m_transitionDuration + 1.0, duration + 1.0)) return;
    m_transitionDuration = duration;
    saveSetting(QStringLiteral("transitionDuration"), duration);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setShuffleEnabled(bool enabled)
{
    if (m_shuffleEnabled == enabled) return;
    m_shuffleEnabled = enabled;
    saveSetting(QStringLiteral("shuffleEnabled"), enabled);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setBeatSensitivity(double sensitivity)
{
    sensitivity = std::clamp(sensitivity, 0.0, 2.0);
    if (qFuzzyCompare(m_beatSensitivity + 1.0, sensitivity + 1.0)) return;
    m_beatSensitivity = sensitivity;
    saveSetting(QStringLiteral("beatSensitivity"), sensitivity);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setHardCutsEnabled(bool enabled)
{
    if (m_hardCutsEnabled == enabled) return;
    m_hardCutsEnabled = enabled;
    saveSetting(QStringLiteral("hardCutsEnabled"), enabled);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setHardCutSensitivity(double sensitivity)
{
    sensitivity = std::clamp(sensitivity, 0.0, 5.0);
    if (qFuzzyCompare(m_hardCutSensitivity + 1.0, sensitivity + 1.0)) return;
    m_hardCutSensitivity = sensitivity;
    saveSetting(QStringLiteral("hardCutSensitivity"), sensitivity);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setHardCutDuration(int duration)
{
    duration = std::clamp(duration, 2, std::max(2, m_presetDuration - 1));
    if (m_hardCutDuration == duration) return;
    m_hardCutDuration = duration;
    saveSetting(QStringLiteral("hardCutDuration"), duration);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setPresetSource(const QString &source)
{
    const QString normalized = normalizedSource(source);
    refreshPresetLibrary();

    if (normalized == QStringLiteral("ALL") && !fullLibraryAvailable()) {
        setStatusMessage(QStringLiteral("Install the full preset pack before selecting the full library"));
        return;
    }
    if (m_presetSource == normalized) return;

    m_presetSource = normalized;
    saveSetting(QStringLiteral("presetSource"), normalized);
    refreshPresetLibrary();
    emit settingsChanged();
    scheduleRestartIfRunning();
}

QString VisualizerLauncher::findProjectMBinary() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString workingDir = QDir::currentPath();

    const QStringList candidatePaths = {
        appDir + QStringLiteral("/../libexec/magnetofon/projectm/bin/projectMSDL"),
        appDir + QStringLiteral("/resources/projectm/linux-x64/bin/projectMSDL"),
        workingDir + QStringLiteral("/resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/../resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/../../resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/projectMSDL"),
        appDir + QStringLiteral("/projectMSDL.exe")
    };

    for (const QString &path : candidatePaths) {
        const QFileInfo info(path);
        if (info.exists() && info.isFile()) {
            qDebug() << "[VisualizerLauncher] Found projectM binary at:" << info.absoluteFilePath();
            return info.absoluteFilePath();
        }
    }

    return QStandardPaths::findExecutable(QStringLiteral("projectMSDL"));
}

int VisualizerLauncher::countPresetFiles(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) return 0;

    QDirIterator iterator(directory,
                          {QStringLiteral("*.milk"), QStringLiteral("*.prjm")},
                          QDir::Files,
                          QDirIterator::Subdirectories);
    int count = 0;
    while (iterator.hasNext()) {
        iterator.next();
        ++count;
    }
    return count;
}

QString VisualizerLauncher::findPresetDirectory(const QString &source) const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString workingDir = QDir::currentPath();
    const QString appDir = QCoreApplication::applicationDirPath();

    QStringList candidates;
    if (normalizedSource(source) == QStringLiteral("CURATED")) {
        candidates = {
            workingDir + QStringLiteral("/visuals/curated/presets"),
            appDir + QStringLiteral("/../share/magnetofon/visuals/curated/presets"),
            appDir + QStringLiteral("/visuals/curated/presets"),
            appDir + QStringLiteral("/../../visuals/curated/presets"),
            workingDir + QStringLiteral("/resources/projectm/linux-x64/presets"),
            appDir + QStringLiteral("/resources/projectm/linux-x64/presets")
        };
    } else {
        candidates = {
            appData + QStringLiteral("/visuals"),
            workingDir + QStringLiteral("/visuals/presets"),
            appDir + QStringLiteral("/../share/magnetofon/visuals/presets"),
            appDir + QStringLiteral("/visuals/presets"),
            appDir + QStringLiteral("/../../visuals/presets")
        };
    }

    for (const QString &candidate : candidates) {
        const QString directory = cleanExistingDirectory(candidate);
        if (!directory.isEmpty() && countPresetFiles(directory) > 0) return directory;
    }
    return {};
}

QString VisualizerLauncher::findTextureDirectory() const
{
    if (m_activePresetDirectory.isEmpty()) return {};

    const QDir presets(m_activePresetDirectory);
    const QStringList candidates = {
        presets.absolutePath() + QStringLiteral("/Textures"),
        presets.absolutePath() + QStringLiteral("/textures"),
        presets.absolutePath() + QStringLiteral("/../Textures"),
        presets.absolutePath() + QStringLiteral("/../textures")
    };
    for (const QString &candidate : candidates) {
        const QString directory = cleanExistingDirectory(candidate);
        if (!directory.isEmpty()) return directory;
    }
    return {};
}

void VisualizerLauncher::refreshPresetLibrary()
{
    const QString curatedDirectory = findPresetDirectory(QStringLiteral("CURATED"));
    const QString fullDirectory = findPresetDirectory(QStringLiteral("ALL"));
    const int curatedCount = countPresetFiles(curatedDirectory);
    const int fullCount = countPresetFiles(fullDirectory);
    const QString activeDirectory = m_presetSource == QStringLiteral("ALL") ? fullDirectory : curatedDirectory;
    const int activeCount = m_presetSource == QStringLiteral("ALL") ? fullCount : curatedCount;

    if (m_curatedPresetCount == curatedCount && m_fullPresetCount == fullCount &&
        m_activePresetDirectory == activeDirectory && m_presetCount == activeCount) {
        return;
    }

    m_curatedPresetCount = curatedCount;
    m_fullPresetCount = fullCount;
    m_activePresetDirectory = activeDirectory;
    m_presetCount = activeCount;
    emit presetLibraryChanged();
}

QString VisualizerLauncher::resolveAudioMonitorSource() const
{
#if defined(Q_OS_LINUX)
    QProcess pactl;
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("get-default-sink")});
    if (pactl.waitForFinished(1200) && pactl.exitStatus() == QProcess::NormalExit && pactl.exitCode() == 0) {
        const QString sink = QString::fromUtf8(pactl.readAllStandardOutput()).trimmed();
        if (!sink.isEmpty()) return sink + QStringLiteral(".monitor");
    }
#endif
    return QStringLiteral("@DEFAULT_MONITOR@");
}

QStringList VisualizerLauncher::buildArguments() const
{
    QStringList args;
    if (!m_activePresetDirectory.isEmpty()) {
        args << QStringLiteral("--presetPath=%1").arg(m_activePresetDirectory);
    }

    const QString textureDirectory = findTextureDirectory();
    if (!textureDirectory.isEmpty()) {
        args << QStringLiteral("--texturePath=%1").arg(textureDirectory);
    }

    args << QStringLiteral("--enableSplash=0")
         << QStringLiteral("--shuffleEnabled=%1").arg(m_shuffleEnabled ? 1 : 0)
         << QStringLiteral("--fps=%1").arg(m_targetFps)
         << QStringLiteral("--presetDuration=%1").arg(m_presetDuration)
         << QStringLiteral("--transitionDuration=%1").arg(QString::number(m_transitionDuration, 'f', 1))
         << QStringLiteral("--beatSensitivity=%1").arg(QString::number(m_beatSensitivity, 'f', 1))
         << QStringLiteral("--hardCutsEnabled=%1").arg(m_hardCutsEnabled ? 1 : 0)
         << QStringLiteral("--hardCutSensitivity=%1").arg(QString::number(m_hardCutSensitivity, 'f', 1))
         << QStringLiteral("--hardCutDuration=%1").arg(m_hardCutDuration);
    return args;

}

void VisualizerLauncher::toggleVisuals()
{
    if (isRunning()) stopVisuals();
    else launchVisuals();
}

void VisualizerLauncher::launchVisuals()
{
    if (isRunning()) return;

    refreshPresetLibrary();
    if (m_presetCount < 1 || m_activePresetDirectory.isEmpty()) {
        setStatusMessage(m_presetSource == QStringLiteral("ALL")
            ? QStringLiteral("Full preset library not found — install it or select Curated")
            : QStringLiteral("No curated presets were found"));
        return;
    }

    const QString binary = findProjectMBinary();
    if (binary.isEmpty()) {
        setStatusMessage(QStringLiteral("projectMSDL runtime not found"));
        return;
    }

    const QFileInfo binaryInfo(binary);
    const QString binaryDirectory = binaryInfo.absolutePath();
    const QString libraryDirectory = QFileInfo(binaryDirectory + QStringLiteral("/../lib")).absoluteFilePath();

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (QDir(libraryDirectory).exists()) {
        const QString existingPath = environment.value(QStringLiteral("LD_LIBRARY_PATH"));
        environment.insert(QStringLiteral("LD_LIBRARY_PATH"),
                           libraryDirectory + (existingPath.isEmpty() ? QString() : QStringLiteral(":") + existingPath));
    }

    if (!m_projectMConfigDirectory) {
        m_projectMConfigDirectory = std::make_unique<QTemporaryDir>(
            QDir::tempPath() + QStringLiteral("/magnetofon-projectm-XXXXXX"));
    }
    if (m_projectMConfigDirectory->isValid()) {
        // Keep projectMSDL's own saved "preset locked" state from disabling Magnetofon's rotation.
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), m_projectMConfigDirectory->path());
    }

#if defined(Q_OS_LINUX)
    const QString monitorSource = resolveAudioMonitorSource();
    environment.insert(QStringLiteral("PULSE_SOURCE"), monitorSource);
    environment.insert(QStringLiteral("SDL_AUDIODRIVER"), QStringLiteral("pulseaudio"));
    if (m_audioSourceName != monitorSource) {
        m_audioSourceName = monitorSource;
        emit runtimeInfoChanged();
    }
#endif

    m_processOutputBuffer.clear();
    m_currentPresetName.clear();
    m_processFailed = false;
    m_process->setProcessEnvironment(environment);
    if (binaryInfo.exists()) m_process->setWorkingDirectory(binaryDirectory);

    qDebug() << "[VisualizerLauncher] Launching" << binary << buildArguments();
    setStatusMessage(QStringLiteral("Starting projectM with %1 presets…").arg(m_presetCount));
    m_process->start(binary, buildArguments());
}

void VisualizerLauncher::requestProcessStop(bool restartAfterStop)
{
    if (!isRunning()) {
        if (restartAfterStop) launchVisuals();
        return;
    }

    m_restartAfterStop = restartAfterStop;
    setStatusMessage(restartAfterStop ? QStringLiteral("Applying visual settings…")
                                      : QStringLiteral("Stopping visualizer…"));
    m_process->terminate();
    m_forceKillTimer.start();
}

void VisualizerLauncher::stopVisuals()
{
    m_restartTimer.stop();
    requestProcessStop(false);
}

void VisualizerLauncher::restartIfRunning()
{
    scheduleRestartIfRunning();
}

void VisualizerLauncher::scheduleRestartIfRunning()
{
    if (isRunning()) m_restartTimer.start();
}

void VisualizerLauncher::performRestart()
{
    requestProcessStop(true);
}

void VisualizerLauncher::onProcessStateChanged(QProcess::ProcessState newState)
{
    emit isRunningChanged();

    if (newState == QProcess::Running) {
        setStatusMessage(QStringLiteral("Running · %1 presets · %2 order")
                             .arg(m_presetCount)
                             .arg(m_shuffleEnabled ? QStringLiteral("shuffled") : QStringLiteral("sequential")));
        return;
    }

    if (newState == QProcess::NotRunning) {
        m_forceKillTimer.stop();
        if (m_restartAfterStop) {
            m_restartAfterStop = false;
            QTimer::singleShot(0, this, &VisualizerLauncher::launchVisuals);
        } else if (!m_processFailed) {
            setStatusMessage(QStringLiteral("Visualizer stopped"));
        }
    }
}

void VisualizerLauncher::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::Crashed &&
        (m_restartAfterStop || m_statusMessage.startsWith(QStringLiteral("Stopping")))) return;
    m_processFailed = true;
    setStatusMessage(QStringLiteral("Visualizer error: %1").arg(m_process->errorString()));
}

void VisualizerLauncher::readProcessOutput()
{
    m_processOutputBuffer += QString::fromUtf8(m_process->readAllStandardOutput());

    qsizetype newline = -1;
    while ((newline = m_processOutputBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_processOutputBuffer.left(newline).trimmed();
        m_processOutputBuffer.remove(0, newline + 1);

        const QString marker = QStringLiteral("Displaying preset: ");
        const qsizetype markerPosition = line.indexOf(marker);
        if (markerPosition >= 0) {
            const QString presetPath = line.mid(markerPosition + marker.size()).trimmed();
            const QString presetName = QFileInfo(presetPath).completeBaseName();
            if (!presetName.isEmpty() && presetName != m_currentPresetName) {
                m_currentPresetName = presetName;
                emit runtimeInfoChanged();
            }
        }
        if (!line.isEmpty()) qDebug().noquote() << "[projectMSDL]" << line;
    }
}

void VisualizerLauncher::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit runtimeInfoChanged();
}
