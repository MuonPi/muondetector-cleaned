#ifndef LOGPLOTSWIDGET_H
#define LOGPLOTSWIDGET_H

#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Ui {
class LogPlotsWidget;
}

struct LogInfoStruct;

class LogBuffer {
public:
    LogBuffer(const QString& a_name) { name = a_name; }
    LogBuffer() = default;
    ~LogBuffer() { }
    void clear() { buffer.clear(); }
    void setName(const QString& a_name) { name = a_name; }
    void setUnit(const QString& a_unit) { unit = a_unit; }
    void push_back(const QPointF& p) { buffer.push_back(p); }
    QPointF& at(int i) { return buffer[i]; }
    const QPointF& operator()(int i) const { return buffer[i]; }
    const QPointF& operator[](int i) const { return buffer[i]; }
    QVector<QPointF>& data() { return buffer; }
    const QString& getName() const { return name; }
    const QString& getUnit() const { return unit; }

private:
    QVector<QPointF> buffer;
    QString name = "";
    QString unit = "";
};

class LogPlotsWidget : public QWidget {
    Q_OBJECT

public:
    explicit LogPlotsWidget(QWidget* parent = 0);
    ~LogPlotsWidget();

public slots:
    void onTemperatureReceived(float temp);
    void onTimeAccReceived(quint32 acc);
    void onUiEnabledStateChange(bool connected);
    void onBiasVoltageCalculated(float ubias);
    void onBiasCurrentCalculated(float ibias);
    void onGpioRatesReceived(quint8 whichrate, QVector<QPointF> rates);
    void onLogInfoReceived(const LogInfoStruct& lis);

private slots:
    void updateLogTable();

    void on_tableWidget_cellClicked(int row, int column);
    void onScalingChanged();
    void on_linesCheckBox_clicked();
    void on_pointSizeSpinBox_valueChanged(int arg1);

private:
    Ui::LogPlotsWidget* ui;
    QMap<QString, LogBuffer> fLogMap;
    QString fCurrentLog = "";
};

#endif // LOGPLOTSWIDGET_H
