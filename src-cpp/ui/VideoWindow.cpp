#include "VideoWindow.hpp"

#include <QCloseEvent>
#include <QHideEvent>
#include <QPalette>
#include <QShowEvent>
#include <QVBoxLayout>

#include <algorithm>

VideoWindow::VideoWindow(QWidget *parent)
    : QWidget(parent)
    , m_surface(new QWidget(this))
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlag(Qt::Window, true);
    setWindowTitle(QStringLiteral("Magnetofon Video"));
    setMinimumSize(480, 270);
    resize(960, 540);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_surface->setAttribute(Qt::WA_NativeWindow);
    m_surface->setAttribute(Qt::WA_OpaquePaintEvent);
    m_surface->setAutoFillBackground(true);
    QPalette palette = m_surface->palette();
    palette.setColor(QPalette::Window, Qt::black);
    m_surface->setPalette(palette);
    layout->addWidget(m_surface);

    // Force creation of the stable native child window that libmpv renders into.
    m_surface->winId();
}

WId VideoWindow::renderTargetId() const
{
    return m_surface ? m_surface->winId() : 0;
}

void VideoWindow::showMedia(const QString &title, int videoWidth, int videoHeight)
{
    setWindowTitle(title.isEmpty() ? QStringLiteral("Magnetofon Video")
                                   : QStringLiteral("Magnetofon — ") + title);

    if (!isVisible() && videoWidth > 0 && videoHeight > 0) {
        constexpr int preferredWidth = 960;
        const double aspect = static_cast<double>(videoWidth) / videoHeight;
        resize(preferredWidth, std::max(360, static_cast<int>(preferredWidth / aspect)));
    }

    show();
    raise();
    activateWindow();
}

void VideoWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void VideoWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit windowVisibilityChanged(false);
}

void VideoWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit windowVisibilityChanged(true);
}
