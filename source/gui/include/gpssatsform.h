#ifndef GPSSATSFORM_H
#define GPSSATSFORM_H

#include <QMap>
#include <QVector>
#include <QWidget>

struct GeodeticPos;
class GnssSatellite;
struct GnssMonHwStruct;
struct GnssMonHw2Struct;

namespace Ui {
class GpsSatsForm;
}

class GpsSatsForm : public QWidget {
    Q_OBJECT

public:
    explicit GpsSatsForm(QWidget* parent = 0);
    ~GpsSatsForm();

public slots:
    void onSatsReceived(const QVector<GnssSatellite>& satlist);
    void onTimeAccReceived(quint32 acc);
    void onFreqAccReceived(quint32 acc);
    void onIntCounterReceived(quint32 cnt);
    void onGpsMonHWReceived(const GnssMonHwStruct& hwstruct);
    void onGpsMonHW2Received(const GnssMonHw2Struct& hw2struct);
    void onGpsVersionReceived(const QString& swString, const QString& hwString, const QString& protString);
    void onGpsFixReceived(quint8 val);
    void onGeodeticPosReceived(const GeodeticPos& pos);
    void onUiEnabledStateChange(bool connected);
    void onUbxUptimeReceived(quint32 val);

private:
    Ui::GpsSatsForm* ui;
    QVector<QPointF> iqTrack;
};

#endif // GPSSATSFORM_H
