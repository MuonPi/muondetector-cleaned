#ifndef CALIBSCANDIALOG_H
#define CALIBSCANDIALOG_H

#include <QDialog>
#include <QVector>

struct CalibStruct;

namespace Ui {
class CalibScanDialog;
}

class CalibScanDialog : public QDialog {
    Q_OBJECT

public:
    explicit CalibScanDialog(QWidget* parent = 0);
    ~CalibScanDialog();

public slots:
    void onCalibReceived(bool valid, bool eepromValid, quint64 id, const QVector<CalibStruct>& calibList);
    void onAdcSampleReceived(uint8_t channel, double value);
private slots:
    void startManualCurrentCalib();
    void transferCurrentCalibCoeffs();

private:
    Ui::CalibScanDialog* ui;
    QVector<CalibStruct> fCalibList;
    bool fAutoCalibRunning = false;
    uint8_t fCurrentCalibRunning = 0;
    float fCurrBias = 0.;
    QVector<QPointF> fPoints1, fPoints2, fPoints3;
    double fSlope1 = 0., fOffs1 = 0.;
    double fSlope2 = 0., fOffs2 = 0.;
    double fLastRSenseHiVoltage = 0.;
    double fLastRSenseLoVoltage = 0.;

    void setCalibParameter(const QString& name, const QString& value);
    QString getCalibParameter(const QString& name);
    void manualCurrentCalibProgress(double vbias, double ibias);
};

#endif // CALIBSCANDIALOG_H
