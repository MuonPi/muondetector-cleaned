#ifndef SCANFORM_H
#define SCANFORM_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>
#include <gpio_pin_definitions.h>
#include <muondetector_structs.h>

// list of possible scan parameters
const static QVector<QString> SP_NAMES = { "VOID", "THR1", "THR2", "BIAS", "DAC4" };
// list of possible observables
const static QVector<QString> OP_NAMES = { "VOID", "UBXRATE" };

namespace Ui {
class ScanForm;
}

class ScanForm : public QWidget {
    Q_OBJECT

public:
    struct ScanPoint {
        double value { 0. };
        double error { 0. };
        double temp { 0. };
    };

    explicit ScanForm(QWidget* parent = 0);
    ~ScanForm();

signals:
    void setThresholdVoltage(uint8_t ch, float val);
    void setBiasControlVoltage(float val);
    void gpioInhibitChanged(bool inhibitState);
    void mqttInhibitChanged(bool inhibitState);
public slots:
    void onTimeMarkReceived(const UbxTimeMarkStruct& tm);
    void onUiEnabledStateChange(bool connected);
    void onDacReadbackReceived(uint8_t channel, float value);

private slots:
    void on_scanStartPushButton_clicked();
    void scanParIteration();
    void adjustScanPar(QString scanParName, double value);
    void finishScan();
    void updateScanPlot();
    void on_plotDifferentialCheckBox_toggled(bool checked);
    void exportData();

private:
    Ui::ScanForm* ui;
    bool active = false;
    UbxTimeMarkStruct lastTM;
    double minRange = 0.;
    double maxRange = 0.;
    double stepSize = 0.001;
    quint8 scanPar = 0;
    quint8 obsPar = 0;
    double currentScanPar = 0.0;
    uint64_t currentCounts;
    double currentTimeInterval = 0.;
    double maxMeasurementTimeInterval = 1.;
    unsigned long maxMeasurementStatistics { 0 };
    bool waitForFirst = false;

    QMap<double, ScanPoint> scanData;
    bool plotDifferential = false;
    double fLastDacs[4] = { 0., 0., 0., 0. };
};

#endif // SCANFORM_H
