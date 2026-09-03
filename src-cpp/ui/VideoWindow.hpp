#ifndef VIDEO_WINDOW_HPP
#define VIDEO_WINDOW_HPP

#include <QWidget>

class QCloseEvent;
class QHideEvent;
class QShowEvent;

class VideoWindow : public QWidget {
    Q_OBJECT

public:
    explicit VideoWindow(QWidget *parent = nullptr);

    WId renderTargetId() const;
    void showMedia(const QString &title, int videoWidth, int videoHeight);

signals:
    void windowVisibilityChanged(bool visible);

protected:
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QWidget *m_surface = nullptr;
};

#endif // VIDEO_WINDOW_HPP
