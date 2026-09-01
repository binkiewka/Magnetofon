#include "VuMeterItem.hpp"

#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

VuMeterItem::VuMeterItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void VuMeterItem::setLeftLevel(double level)
{
    level = std::clamp(level, 0.0, 1.1);
    if (std::abs(m_leftLevel - level) > 0.002) {
        m_leftLevel = level;
        emit leftLevelChanged();
        update();
    }
}

void VuMeterItem::setRightLevel(double level)
{
    level = std::clamp(level, 0.0, 1.1);
    if (std::abs(m_rightLevel - level) > 0.002) {
        m_rightLevel = level;
        emit rightLevelChanged();
        update();
    }
}

void VuMeterItem::paint(QPainter *p)
{
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double w = width();
    const double h = height();

    QLinearGradient frame(0, 0, 0, h);
    frame.setColorAt(0.0, QColor(62, 72, 69));
    frame.setColorAt(0.08, QColor(26, 33, 31));
    frame.setColorAt(0.92, QColor(7, 11, 12));
    frame.setColorAt(1.0, QColor(0, 2, 3));
    p->setPen(QPen(QColor(95, 112, 107, 150), 1));
    p->setBrush(frame);
    p->drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 5, 5);

    const double gap = 7.0;
    const double meterW = (w - gap - 8.0) / 2.0;
    drawMcIntoshDial(p, 3.0, 3.0, meterW, h - 6.0, m_leftLevel, "LEFT POWER OUTPUT");
    drawMcIntoshDial(p, 3.0 + meterW + gap, 3.0, meterW, h - 6.0, m_rightLevel, "RIGHT POWER OUTPUT");

    QLinearGradient glassLine(0, 0, w, 0);
    glassLine.setColorAt(0.0, QColor(0, 175, 255, 50));
    glassLine.setColorAt(0.5, QColor(70, 225, 255, 115));
    glassLine.setColorAt(1.0, QColor(0, 175, 255, 50));
    p->setPen(QPen(glassLine, 1));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(1.5, 1.5, w - 3, h - 3), 4, 4);
}

void VuMeterItem::drawMcIntoshDial(QPainter *p, double x, double y, double w, double h,
                                    double level, const QString &label)
{
    p->save();
    p->translate(x, y);

    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 150));
    p->drawRoundedRect(QRectF(2, 3, w - 1, h - 1), 4, 4);

    QLinearGradient bezel(0, 0, 0, h);
    bezel.setColorAt(0.0, QColor(76, 91, 88));
    bezel.setColorAt(0.12, QColor(19, 27, 28));
    bezel.setColorAt(0.9, QColor(4, 8, 10));
    bezel.setColorAt(1.0, QColor(49, 58, 55));
    p->setPen(QPen(QColor(89, 103, 99), 1));
    p->setBrush(bezel);
    p->drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 4, 4);

    const QRectF faceRect(3.0, 3.0, w - 6.0, h - 7.0);
    QPainterPath facePath;
    facePath.addRoundedRect(faceRect, 3, 3);
    p->setClipPath(facePath);

    QRadialGradient face(w * 0.48, h * 0.43, w * 0.68);
    face.setColorAt(0.0, QColor(0, 177, 235));
    face.setColorAt(0.34, QColor(0, 117, 185));
    face.setColorAt(0.73, QColor(0, 56, 102));
    face.setColorAt(1.0, QColor(2, 19, 31));
    p->setPen(Qt::NoPen);
    p->setBrush(face);
    p->drawRect(faceRect);

    p->setPen(QPen(QColor(20, 88, 125, 80), 0.7));
    for (int i = 1; i < 7; ++i) {
        const double gy = faceRect.top() + i * faceRect.height() / 7.0;
        p->drawLine(QPointF(faceRect.left(), gy), QPointF(faceRect.right(), gy));
    }
    for (int i = 1; i < 9; ++i) {
        const double gx = faceRect.left() + i * faceRect.width() / 9.0;
        p->drawLine(QPointF(gx, faceRect.top()), QPointF(gx, faceRect.bottom()));
    }

    const double cx = w / 2.0;
    const double cy = h * 1.02;
    const double radius = h * 0.72;
    const double startAngle = 135.0;
    const double spanAngle = -90.0;
    const double currentAngle = startAngle + spanAngle * std::min(1.05, level);

    const QRectF arcRect(cx - radius, cy - radius, radius * 2.0, radius * 2.0);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(225, 246, 255, 230), 1.4));
    p->drawArc(arcRect, 45 * 16, 90 * 16);

    static const char *scaleLabels[] = {"−40", "−20", "−10", "−5", "−3", "0", "+3"};
    QFont scaleFont("DejaVu Sans Mono");
    scaleFont.setPixelSize(std::max(6, static_cast<int>(h * 0.075)));
    scaleFont.setWeight(QFont::DemiBold);
    p->setFont(scaleFont);

    for (int i = 0; i < 7; ++i) {
        const double norm = i / 6.0;
        const double degrees = startAngle + spanAngle * norm;
        const double angle = degrees * M_PI / 180.0;
        const bool danger = i >= 5;
        const QColor tickColor = danger ? QColor(255, 70, 100, 235) : QColor(225, 246, 255, 225);

        const QPointF outer(cx + std::cos(angle) * (radius + 1),
                            cy - std::sin(angle) * (radius + 1));
        const QPointF inner(cx + std::cos(angle) * (radius - (i % 3 == 0 ? 9 : 7)),
                            cy - std::sin(angle) * (radius - (i % 3 == 0 ? 9 : 7)));
        p->setPen(QPen(tickColor, i == 6 ? 1.8 : 1.2));
        p->drawLine(inner, outer);

        const double labelRadius = radius - 18;
        const QPointF lp(cx + std::cos(angle) * labelRadius,
                         cy - std::sin(angle) * labelRadius);
        p->setPen(tickColor);
        p->drawText(QRectF(lp.x() - 12, lp.y() - 6, 24, 12), Qt::AlignCenter,
                    QString::fromUtf8(scaleLabels[i]));
    }

    QFont titleFont("Noto Sans");
    titleFont.setPixelSize(std::max(7, static_cast<int>(h * 0.082)));
    titleFont.setWeight(QFont::DemiBold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.45);
    p->setFont(titleFont);
    p->setPen(QColor(225, 247, 255, 225));
    p->drawText(QRectF(9, 6, w - 18, 14), Qt::AlignCenter, label);

    QFont unitFont("DejaVu Sans Mono");
    unitFont.setPixelSize(std::max(6, static_cast<int>(h * 0.065)));
    unitFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    p->setFont(unitFont);
    p->setPen(QColor(120, 212, 239, 190));
    p->drawText(QRectF(0, h - 20, w, 12), Qt::AlignCenter, "WATTS  /  8Ω");

    const double needleAngle = currentAngle * M_PI / 180.0;
    const QPointF needleEnd(cx + std::cos(needleAngle) * (radius + 3),
                            cy - std::sin(needleAngle) * (radius + 3));
    p->setPen(QPen(QColor(0, 0, 0, 130), 3.5));
    p->drawLine(QPointF(cx + 1.5, cy + 2), QPointF(needleEnd.x() + 1.5, needleEnd.y() + 2));
    p->setPen(QPen(QColor(255, 43, 78), 2.0));
    p->drawLine(QPointF(cx, cy), needleEnd);

    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 18));
    QPainterPath reflection;
    reflection.moveTo(faceRect.left(), faceRect.top());
    reflection.lineTo(faceRect.right(), faceRect.top());
    reflection.lineTo(faceRect.right() * 0.78, faceRect.top() + faceRect.height() * 0.32);
    reflection.lineTo(faceRect.left() + faceRect.width() * 0.13, faceRect.top() + faceRect.height() * 0.23);
    reflection.closeSubpath();
    p->drawPath(reflection);

    const bool peak = level > 0.88;
    QRadialGradient led(w - 12, 12, 5);
    led.setColorAt(0.0, peak ? QColor(255, 133, 151) : QColor(59, 13, 22));
    led.setColorAt(0.45, peak ? QColor(255, 42, 76) : QColor(31, 8, 13));
    led.setColorAt(1.0, QColor(5, 7, 7));
    p->setBrush(led);
    p->setPen(QPen(peak ? QColor(255, 105, 128) : QColor(83, 24, 33), 0.8));
    p->drawEllipse(QPointF(w - 12, 12), 3.2, 3.2);

    p->setClipping(false);
    p->restore();
}
