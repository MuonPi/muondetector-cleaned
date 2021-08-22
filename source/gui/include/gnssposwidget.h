#ifndef GNSSPOSWIDGET_H
#define GNSSPOSWIDGET_H

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QPainterPath>
#include <QPixmap>
#include <QVector>
#include <QWidget>

class GnssSatellite;

constexpr int DEFAULT_CONTROL_POINTS = 5;

namespace Ui {
class GnssPosWidget;
}

struct SatHistoryPoint {
    QPointF posCart;
    QPoint posPolar;
    quint8 satId;
    quint8 gnssId;
    quint8 cnr;
    QColor color;
    QDateTime time;
};

class GnssPosWidget : public QWidget {
    Q_OBJECT

public:
    explicit GnssPosWidget(QWidget* parent = 0);
    ~GnssPosWidget();
    void resizeEvent(QResizeEvent* event);

public slots:
    void onSatsReceived(const QVector<GnssSatellite>& satlist);
    void replot();
    void onUiEnabledStateChange(bool connected);

private slots:
    void on_satSizeSpinBox_valueChanged(int arg1);
    void popUpMenu(const QPoint&);
    void exportToFile();

private:
    Ui::GnssPosWidget* ui;
    QMap<int, QHash<QPoint, QVector<SatHistoryPoint>>> satTracks;
    QVector<GnssSatellite> fCurrentSatlist;

    QPointF polar2cartUnity(const QPointF& pol);
    QPolygonF getPolarUnitPolygon(const QPointF& pos, int controlPoints = DEFAULT_CONTROL_POINTS);
    QPolygonF getCartPolygonUnity(const QPointF& polarPos);
    void drawPolarPixMap(QPixmap& pm);
    void drawCartesianPixMap(QPixmap& pm);
    int alphaFromCnr(int cnr);
};

inline uint qHash(const QPoint& p)
{
    return 1000 * p.x() + p.y();
}

#endif // GNSSPOSWIDGET_H
