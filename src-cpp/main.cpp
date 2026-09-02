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
    app.setApplicationVersion("2.0.0");

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

    // Connect playlist track selection directly to audio player in C++

    QObject::connect(&playlist, &PlaylistModel::trackSelected, &player, [&player](const QString &filePath) {
        player.load(filePath);
        player.play();
    });

    QObject::connect(&player, &AudioPlayer::trackEnded, &playlist, [&playlist]() {
        playlist.nextTrack();
    });

    QObject::connect(&playlist, &PlaylistModel::emptied, &player, &AudioPlayer::stop);

    const QStringList arguments = QCoreApplication::arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        playlist.addFile(QFileInfo(arguments.at(i)).absoluteFilePath());
    }

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
