#ifndef VU_METER_ITEM_HPP
#define VU_METER_ITEM_HPP

#include <QQuickPaintedItem>
#include <QPainter>

class VuMeterItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(double leftLevel READ leftLevel WRITE setLeftLevel NOTIFY leftLevelChanged)
    Q_PROPERTY(double rightLevel READ rightLevel WRITE setRightLevel NOTIFY rightLevelChanged)

public:
    explicit VuMeterItem(QQuickItem *parent = nullptr);

    double leftLevel() const { return m_leftLevel; }
    double rightLevel() const { return m_rightLevel; }

    void setLeftLevel(double level);
    void setRightLevel(double level);

    void paint(QPainter *painter) override;

signals:
    void leftLevelChanged();
    void rightLevelChanged();

private:
    void drawMcIntoshDial(QPainter *p, double x, double y, double w, double h, double level, const QString &label);

    double m_leftLevel = 0.0;
    double m_rightLevel = 0.0;
};

#endif // VU_METER_ITEM_HPP
