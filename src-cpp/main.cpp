#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QWindow>
#include <QTimer>
#include <QImage>
#include <QDebug>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

#include "AudioPlayer.hpp"
#include "PlaylistModel.hpp"
#include "PresetPackDownloader.hpp"
#include "VisualizerLauncher.hpp"
#include "VuMeterItem.hpp"


#include <clocale>
#include <vector>

#ifdef MAGNETOFON_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace {

#ifdef MAGNETOFON_HAVE_X11
void publishX11WindowIcon(QWindow *window, const QImage &source)
{
    if (!window || source.isNull() || QGuiApplication::platformName() != QStringLiteral("xcb")) return;

    Display *display = XOpenDisplay(nullptr);
    if (!display) return;

    const QImage image = source.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_ARGB32);
    std::vector<unsigned long> property;
    property.reserve(static_cast<size_t>(image.width() * image.height()) + 2);
    property.push_back(static_cast<unsigned long>(image.width()));
    property.push_back(static_cast<unsigned long>(image.height()));
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) property.push_back(line[x]);
    }

    const Atom iconAtom = XInternAtom(display, "_NET_WM_ICON", False);
    XChangeProperty(display, static_cast<::Window>(window->winId()), iconAtom, XA_CARDINAL, 32,
                    PropModeReplace, reinterpret_cast<const unsigned char *>(property.data()),
                    static_cast<int>(property.size()));
    XFlush(display);
    XCloseDisplay(display);
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    // libmpv requires LC_NUMERIC set to "C" for consistent dot decimal parsing
    std::setlocale(LC_NUMERIC, "C");

    // Force OpenGL hardware acceleration
    qputenv("QSG_RHI_BACKEND", "opengl");
    qputenv("PULSE_PROP_application.name", "Magnetofon");
    qputenv("PULSE_PROP_application.icon_name", "magnetofon");
    qputenv("PULSE_PROP_media.role", "music");

    QApplication app(argc, argv);
    app.setApplicationName("Magnetofon");
    app.setApplicationDisplayName("Magnetofon");
    app.setDesktopFileName("magnetofon.desktop");
    app.setOrganizationName("Magnetofon");
    app.setApplicationVersion(QString::fromLatin1(MAGNETOFON_VERSION));

    QStringList incomingPaths;
    for (const QString &argument : app.arguments().mid(1)) {
        const QUrl url(argument);
        incomingPaths.append(QFileInfo(url.isLocalFile() ? url.toLocalFile() : argument).absoluteFilePath());
    }
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString endpoint = runtime + QStringLiteral("/magnetofon-player");
    QLockFile instanceLock(endpoint + QStringLiteral(".lock"));
    if (!instanceLock.tryLock()) {
        QLocalSocket socket;
        for (int attempt = 0; attempt < 50; ++attempt) {
            socket.connectToServer(endpoint);
            if (socket.waitForConnected(100)) break;
            socket.abort();
            QThread::msleep(100);
        }
        if (socket.state() != QLocalSocket::ConnectedState) {
            qCritical() << "Could not contact the running Magnetofon instance";
            return 1;
        }
        socket.write(QJsonDocument(QJsonArray::fromStringList(incomingPaths)).toJson(QJsonDocument::Compact) + '\n');
        if (!socket.waitForBytesWritten(3000) || !socket.waitForReadyRead(5000)) return 1;
        return socket.readAll().startsWith("OK") ? 0 : 1;
    }
    QLocalServer::removeServer(endpoint);
    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(endpoint)) {
        qCritical() << "Cannot listen for file-open requests:" << server.errorString();
        return 1;
    }

    const QIcon appIcon(QStringLiteral(":/resources/icon-256.png"));
    const QImage appIconImage(QStringLiteral(":/resources/icon-256.png"));
    app.setWindowIcon(appIcon);

    QQuickStyle::setStyle("Basic");

    // Register custom C++ UI type for QML
    qmlRegisterType<VuMeterItem>("Magnetofon", 1, 0, "VuMeterItem");

    AudioPlayer player;
    PlaylistModel playlist;
    PresetPackDownloader packDownloader;
    VisualizerLauncher visualizerLauncher;

    // Keep shutdown deterministic even while auxiliary video/visualizer windows
    // are open. AudioPlayer and VisualizerLauncher destructors perform the final
    // synchronous libmpv/process cleanup after the event loop exits.
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &player, &AudioPlayer::stop);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &visualizerLauncher, &VisualizerLauncher::stopVisuals);

    QObject::connect(&packDownloader, &PresetPackDownloader::isInstalledChanged,
                     &visualizerLauncher, &VisualizerLauncher::refreshPresetLibrary);

    player.setPlaylist(&playlist);

    for (const QString &path : incomingPaths) playlist.addFile(path);
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&]() {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            const auto receive = [&, socket]() {
                if (!socket->canReadLine()) return;
                const auto document = QJsonDocument::fromJson(socket->readLine());
                if (!document.isArray()) { socket->disconnectFromServer(); return; }
                for (const auto &path : document.array()) playlist.addFile(path.toString());
                for (QWindow *window : app.topLevelWindows()) {
                    if (window->title().startsWith("MAGNETOFON")) {
                        if (window->visibility() == QWindow::Minimized) window->showNormal();
                        window->raise();
                        window->requestActivate();
                    }
                }
                socket->write("OK\n");
                socket->flush();
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &app, receive);
            receive();
        }
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("audioPlayer", &player);
    engine.rootContext()->setContextProperty("playlistModel", &playlist);
    engine.rootContext()->setContextProperty("presetPackDownloader", &packDownloader);
    engine.rootContext()->setContextProperty("visualizerLauncher", &visualizerLauncher);



    const QUrl url(QStringLiteral("qrc:/ui/qml/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    for (QObject *rootObject : engine.rootObjects()) {
        if (auto *rootWindow = qobject_cast<QWindow *>(rootObject)) {
            QTimer::singleShot(0, rootWindow, [rootWindow, appIcon, appIconImage]() {
                rootWindow->setIcon(QIcon());
                rootWindow->setIcon(appIcon);
#ifdef MAGNETOFON_HAVE_X11
                publishX11WindowIcon(rootWindow, appIconImage);
#endif
            });
        }
    }

    qDebug() << "[Magnetofon Native C++] Initialized successfully with Qt 6 & libmpv";

    return app.exec();
}
