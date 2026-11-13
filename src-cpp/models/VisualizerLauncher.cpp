#include "VisualizerLauncher.hpp"
#include "PresetPackDownloader.hpp"

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
    if (source.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0) return QStringLiteral("ALL");
    if (source.compare(QStringLiteral("USER"), Qt::CaseInsensitive) == 0) return QStringLiteral("USER");
    return QStringLiteral("CURATED");
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

    m_audioFreshnessTimer.setInterval(500);
    connect(&m_audioFreshnessTimer, &QTimer::timeout, this, [this]() {
        if (m_audioLive && (!m_audioHeartbeat.isValid() || m_audioHeartbeat.elapsed() > 2000))
            setAudioState(QStringLiteral("unavailable"));
    });

    if (restoreSettings) loadSettings();
    refreshPresetLibrary();

    if ((m_presetSource == QStringLiteral("ALL") && !fullLibraryAvailable()) ||
        (m_presetSource == QStringLiteral("USER") && !userLibraryAvailable())) {
        m_presetSource = QStringLiteral("CURATED");
        saveSetting(QStringLiteral("presetSource"), m_presetSource);
        refreshPresetLibrary();
    }
}

VisualizerLauncher::~VisualizerLauncher()
{
    m_wantsRunning = false;
    m_launchPending = false;
    ++m_launchGeneration;
    m_restartTimer.stop();
    m_audioFreshnessTimer.stop();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
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
                                      0.5, std::min(30.0, m_presetDuration - 0.5));
    m_shuffleEnabled = settings.value(QStringLiteral("shuffleEnabled"), m_shuffleEnabled).toBool();
    m_borderlessWindow = settings.value(QStringLiteral("borderlessWindow"), m_borderlessWindow).toBool();
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
    m_transitionDuration = std::clamp(m_transitionDuration, 0.5, m_presetDuration - 0.5);
    m_hardCutDuration = std::min(m_hardCutDuration, std::max(2, m_presetDuration - 1));
    saveSetting(QStringLiteral("presetDuration"), m_presetDuration);
    saveSetting(QStringLiteral("transitionDuration"), m_transitionDuration);
    saveSetting(QStringLiteral("hardCutDuration"), m_hardCutDuration);
    emit settingsChanged();
    scheduleRestartIfRunning();
}

void VisualizerLauncher::setTransitionDuration(double duration)
{
    duration = std::clamp(duration, 0.5, std::min(30.0, m_presetDuration - 0.5));
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

void VisualizerLauncher::setBorderlessWindow(bool enabled)
{
    if (m_borderlessWindow == enabled) return;
    m_borderlessWindow = enabled;
    saveSetting(QStringLiteral("borderlessWindow"), enabled);
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
    if (normalized == QStringLiteral("USER") && !userLibraryAvailable()) {
        setStatusMessage(QStringLiteral("No user presets found in the writable visuals folder"));
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
#if defined(Q_OS_WIN)
        appDir + QStringLiteral("/projectMSDL.exe"),
        appDir + QStringLiteral("/resources/projectm/windows-x64/bin/projectMSDL.exe"),
        workingDir + QStringLiteral("/resources/projectm/windows-x64/bin/projectMSDL.exe"),
#else
        appDir + QStringLiteral("/../libexec/magnetofon/projectm/bin/projectMSDL"),
        appDir + QStringLiteral("/resources/projectm/linux-x64/bin/projectMSDL"),
        workingDir + QStringLiteral("/resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/../resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/../../resources/projectm/linux-x64/bin/projectMSDL"),
        appDir + QStringLiteral("/projectMSDL"),
#endif
    };

    for (const QString &path : candidatePaths) {
        const QFileInfo info(path);
        if (info.exists() && info.isFile() && info.isExecutable()) {
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

namespace {

bool hasAnyPresetFile(const QString &directory)
{
    if (directory.isEmpty() || !QDir(directory).exists()) return false;

    QDirIterator iterator(directory,
                          {QStringLiteral("*.milk"), QStringLiteral("*.prjm")},
                          QDir::Files,
                          QDirIterator::Subdirectories);
    return iterator.hasNext();
}

} // namespace

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
    } else if (normalizedSource(source) == QStringLiteral("USER")) {
        candidates = {appData + QStringLiteral("/visuals")};
    } else {
        // Only the committed version is active; staging/old versions must never
        // become a library merely because they contain a few extracted presets.
        const QString installed = PresetPackDownloader::installedPackDirectory(
            appData + QStringLiteral("/visuals"));
        if (!installed.isEmpty()) return installed;
        return {}; // Loose custom files are USER, never a verified full pack.
    }

    for (const QString &candidate : candidates) {
        const QString directory = cleanExistingDirectory(candidate);
        if (!directory.isEmpty() && hasAnyPresetFile(directory)) return directory;
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
    const bool available = !findProjectMBinary().isEmpty();
    const QString unavailableReason = available ? QString() :
#if defined(Q_OS_WIN)
        QStringLiteral("Native visuals are not included in this Windows package. Install a supported projectMSDL runtime beside the app to enable them.");
#else
        QStringLiteral("Native visuals are unavailable: install the bundled projectMSDL runtime.");
#endif
    if (m_runtimeAvailable != available || m_runtimeUnavailableReason != unavailableReason) {
        m_runtimeAvailable = available;
        m_runtimeUnavailableReason = unavailableReason;
        emit runtimeInfoChanged();
    }
    const QString curatedDirectory = findPresetDirectory(QStringLiteral("CURATED"));
    const QString fullDirectory = findPresetDirectory(QStringLiteral("ALL"));
    const QString userDirectory = findPresetDirectory(QStringLiteral("USER"));
    const int curatedCount = countPresetFiles(curatedDirectory);
    const int fullCount = countPresetFiles(fullDirectory);
    const int userCount = countPresetFiles(userDirectory);
    const QString activeDirectory = m_presetSource == QStringLiteral("ALL") ? fullDirectory :
        (m_presetSource == QStringLiteral("USER") ? userDirectory : curatedDirectory);
    const int activeCount = m_presetSource == QStringLiteral("ALL") ? fullCount :
        (m_presetSource == QStringLiteral("USER") ? userCount : curatedCount);

    if (m_curatedPresetCount == curatedCount && m_fullPresetCount == fullCount &&
        m_userPresetCount == userCount && m_activePresetDirectory == activeDirectory && m_presetCount == activeCount) {
        return;
    }

    m_curatedPresetCount = curatedCount;
    m_fullPresetCount = fullCount;
    m_userPresetCount = userCount;
    m_activePresetDirectory = activeDirectory;
    m_presetCount = activeCount;
    emit presetLibraryChanged();
}

void VisualizerLauncher::setAudioState(const QString &state)
{
    const bool live = state == QStringLiteral("live") || state == QStringLiteral("silent");
    const QString description = live
        ? (state == QStringLiteral("silent") ? QStringLiteral("Audio live · digital silence")
                                            : QStringLiteral("Audio live · Magnetofon playback"))
        : (isRunning() ? QStringLiteral("Audio unavailable · play music or check your output")
                       : QStringLiteral("Audio idle"));
    if (m_audioLive == live && m_audioStatus == description) return;
    m_audioLive = live;
    m_audioStatus = description;
    emit runtimeInfoChanged();
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
         << QStringLiteral("--borderless=%1").arg(m_borderlessWindow ? 1 : 0)
         << QStringLiteral("--shuffleEnabled=%1").arg(m_shuffleEnabled ? 1 : 0)
         << QStringLiteral("--fps=%1").arg(m_targetFps)
         << QStringLiteral("--presetDuration=%1").arg(m_presetDuration)
         << QStringLiteral("--transitionDuration=%1").arg(QString::number(m_transitionDuration, 'f', 1))
         << QStringLiteral("--beatSensitivity=%1").arg(QString::number(m_beatSensitivity, 'f', 1))
         << QStringLiteral("--hardCutsEnabled=%1").arg(m_hardCutsEnabled ? 1 : 0)
         << QStringLiteral("--hardCutSensitivity=%1").arg(QString::number(m_hardCutSensitivity, 'f', 1))
         << QStringLiteral("--hardCutDuration=%1").arg(m_hardCutDuration);
#if defined(Q_OS_LINUX)
    args << QStringLiteral("--magnetofonPid=%1").arg(QCoreApplication::applicationPid());
#endif
    return args;

}

void VisualizerLauncher::toggleVisuals()
{
    if (m_wantsRunning || isRunning()) stopVisuals();
    else launchVisuals();
}

void VisualizerLauncher::launchVisuals()
{
    if (isRunning() || m_launchPending) return;
    refreshPresetLibrary();
    if (!m_runtimeAvailable) {
        setStatusMessage(m_runtimeUnavailableReason);
        return;
    }
    m_wantsRunning = true;
    ++m_launchGeneration;
    startPreparedVisuals();
}

void VisualizerLauncher::startPreparedVisuals()
{
    if (!m_wantsRunning || isRunning()) return;
    refreshPresetLibrary();
    if (m_presetCount < 1 || m_activePresetDirectory.isEmpty()) {
        m_wantsRunning = false;
        setStatusMessage(m_presetSource == QStringLiteral("ALL")
            ? QStringLiteral("Full preset library not found — install it or select Curated")
            : (m_presetSource == QStringLiteral("USER")
               ? QStringLiteral("No user presets found in the writable visuals folder")
               : QStringLiteral("No curated presets were found")));
        return;
    }

    const QString binary = findProjectMBinary();
    if (binary.isEmpty()) {
        m_wantsRunning = false;
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

    // The helper's process-restricted capture never reads default-monitor or
    // microphone environment hints. It follows this player's actual sink input.
    environment.remove(QStringLiteral("PULSE_SOURCE"));
    m_audioHeartbeat.invalidate();
    setAudioState(QStringLiteral("unavailable"));
    m_audioFreshnessTimer.start();

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
    m_restartAfterStop = restartAfterStop && m_wantsRunning;
    if (!isRunning()) {
        if (m_restartAfterStop) {
            m_restartAfterStop = false;
            launchVisuals();
        }
        return;
    }
    setStatusMessage(restartAfterStop ? QStringLiteral("Applying visual settings…")
                                      : QStringLiteral("Stopping visualizer…"));
    m_process->terminate();
    m_forceKillTimer.start();
}

void VisualizerLauncher::stopVisuals()
{
    // Invalidate queued launches even if the helper has already become NotRunning.
    m_wantsRunning = false;
    m_launchPending = false;
    ++m_launchGeneration;
    m_restartTimer.stop();
    m_audioFreshnessTimer.stop();
    setAudioState(QStringLiteral("unavailable"));
    requestProcessStop(false);
    if (!isRunning()) setStatusMessage(QStringLiteral("Visualizer stopped"));
}

void VisualizerLauncher::restartIfRunning()
{
    scheduleRestartIfRunning();
}

void VisualizerLauncher::scheduleRestartIfRunning()
{
    if (m_wantsRunning && isRunning()) m_restartTimer.start();
}

void VisualizerLauncher::performRestart()
{
    if (m_wantsRunning) requestProcessStop(true);
}

void VisualizerLauncher::onProcessStateChanged(QProcess::ProcessState newState)
{
    emit isRunningChanged();

    if (newState == QProcess::Running) {
        if (!m_wantsRunning) return; // A signal consumer may have just requested Stop.
        setAudioState(QStringLiteral("unavailable"));
        setStatusMessage(QStringLiteral("Rendering · %1 presets · %2 order")
                             .arg(m_presetCount)
                             .arg(m_shuffleEnabled ? QStringLiteral("shuffled") : QStringLiteral("sequential")));
        return;
    }

    if (newState == QProcess::NotRunning) {
        m_audioFreshnessTimer.stop();
        setAudioState(QStringLiteral("unavailable"));
        m_forceKillTimer.stop();
        if (m_restartAfterStop && m_wantsRunning) {
            m_restartAfterStop = false;
            const quint64 generation = m_launchGeneration;
            QTimer::singleShot(0, this, [this, generation]() {
                if (m_wantsRunning && generation == m_launchGeneration) launchVisuals();
            });
        } else {
            m_wantsRunning = false;
            m_launchPending = false;
            m_audioFreshnessTimer.stop();
            if (!m_processFailed) setStatusMessage(QStringLiteral("Visualizer stopped"));
        }
    }
}

void VisualizerLauncher::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::Crashed &&
        (m_restartAfterStop || m_statusMessage.startsWith(QStringLiteral("Stopping")))) return;
    m_processFailed = true;
    m_wantsRunning = false;
    m_launchPending = false;
    ++m_launchGeneration;
    m_audioFreshnessTimer.stop();
    setStatusMessage(QStringLiteral("Visualizer error: %1").arg(m_process->errorString()));
}

void VisualizerLauncher::readProcessOutput()
{
    m_processOutputBuffer += QString::fromUtf8(m_process->readAllStandardOutput());
    if (m_processOutputBuffer.size() > 65536) {
        m_processOutputBuffer = m_processOutputBuffer.right(16384);
    }

    qsizetype newline = -1;
    while ((newline = m_processOutputBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_processOutputBuffer.left(newline).trimmed();
        m_processOutputBuffer.remove(0, newline + 1);

        const QString audioMarker = QStringLiteral("MAGNETOFON_AUDIO ");
        if (line.startsWith(audioMarker)) {
            const QString state = line.mid(audioMarker.size()).section(QLatin1Char(' '), 0, 0);
            if (state == QStringLiteral("live") || state == QStringLiteral("silent") || state == QStringLiteral("unavailable")) {
                m_audioHeartbeat.restart();
                setAudioState(state);
            }
        }
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
