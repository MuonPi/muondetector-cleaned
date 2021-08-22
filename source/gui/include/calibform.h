#ifndef CALIBFORM_H
#define CALIBFORM_H

#include <QVector>
#include <QWidget>
#include <string>

struct CalibStruct;
class CalibScanDialog;

namespace Ui {
class CalibForm;
}

class CalibForm : public QWidget {
    Q_OBJECT

public:
    explicit CalibForm(QWidget* parent = 0);
    ~CalibForm();
signals:
    void calibRequest();
    void writeCalibToEeprom();
    void setBiasDacVoltage(float val);
    void setDacVoltage(uint8_t ch, float val);
    void updatedCalib(const QVector<CalibStruct>& items);
    void setBiasSwitch(bool state);

public slots:
    void onCalibReceived(bool valid, bool eepromValid, quint64 id, const QVector<CalibStruct>& calibList);
    void onAdcSampleReceived(uint8_t channel, float value);
    QString getCalibParameter(const QString& name);
    const CalibStruct& getCalibItem(const QString& name);
    void setCalibParameter(const QString& name, const QString& value);
    bool voltageCalibValid();
    bool currentCalibValid();
    void onUiEnabledStateChange(bool connected);
    void updateCalibTable();

private slots:
    void on_readCalibPushButton_clicked();
    void on_writeEepromPushButton_clicked();
    void doFit();
    void on_calibItemTableWidget_cellChanged(int row, int column);

private:
    Ui::CalibForm* ui;
    QVector<CalibStruct> fCalibList;
    QVector<QPointF> fPoints1, fPoints2, fPoints3;
    double fSlope1 = 0., fOffs1 = 0.;
    double fSlope2 = 0., fOffs2 = 0.;

    CalibScanDialog* calScan = nullptr;
};

#endif // CALIBFORM_H
