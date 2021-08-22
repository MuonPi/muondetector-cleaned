#ifndef PARAMETERMONITORFORM_H
#define PARAMETERMONITORFORM_H

#include <QWidget>
#include <gpio_pin_definitions.h>

struct CalibStruct;
struct UbxTimeMarkStruct;

namespace Ui {
class ParameterMonitorForm;
}

class ParameterMonitorForm : public QWidget {
    Q_OBJECT

public:
    explicit ParameterMonitorForm(QWidget* parent = 0);
    ~ParameterMonitorForm();

signals:
    void setDacVoltage(uint8_t ch, float val);
    void biasVoltageCalculated(float vbias);
    void biasCurrentCalculated(float ibias);
    void adcModeChanged(quint8 mode);
    void gpioInhibitChanged(bool inhibitState);
    void mqttInhibitChanged(bool inhibitState);
    void biasEnableChanged(bool state);
    void preamp1EnableChanged(bool state);
    void preamp2EnableChanged(bool state);
    void polarityChanged(bool pol1, bool pol2);
    void timingSelectionChanged(uint8_t sel);
    void triggerSelectionChanged(GPIO_PIN signal);
    void gainSwitchChanged(bool state);

public slots:
    void onCalibReceived(bool valid, bool eepromValid, quint64 id, const QVector<CalibStruct>& calibList);
    void onAdcSampleReceived(uint8_t channel, float value);
    void onDacReadbackReceived(uint8_t channel, float value);
    void onInputSwitchReceived(uint8_t index);
    void onBiasSwitchReceived(bool state);
    void onPreampSwitchReceived(uint8_t channel, bool state);
    void onTriggerSelectionReceived(GPIO_PIN signal);
    void onGainSwitchReceived(bool state);
    void onTemperatureReceived(float temp);
    void onTimepulseReceived();
    void onAdcTraceReceived(const QVector<float>& sampleBuffer);
    void onTimeAccReceived(quint32 acc);
    void onFreqAccReceived(quint32 acc);
    void onIntCounterReceived(quint32 cnt);
    void onTimeMarkReceived(const UbxTimeMarkStruct& tm);
    void onPolaritySwitchReceived(bool pol1, bool pol2);
    void onUiEnabledStateChange(bool connected);

private slots:
    void on_dacSpinBox1_valueChanged(double arg1);
    void on_dacSpinBox2_valueChanged(double arg1);
    void on_dacSpinBox3_valueChanged(double arg1);
    void on_dacSpinBox4_valueChanged(double arg1);
    void on_dacSlider1_valueChanged(int value);
    void on_dacSlider2_valueChanged(int value);
    void on_dacSlider3_valueChanged(int value);
    void on_dacSlider4_valueChanged(int value);
    void on_gpioInhibitCheckBox_clicked(bool checked);
    void onPolarityCheckBoxClicked(bool checked);
    void on_timingSelectionComboBox_currentIndexChanged(int index);
    void on_adcTriggerSelectionComboBox_currentIndexChanged(int index);

private:
    Ui::ParameterMonitorForm* ui;
    QVector<CalibStruct> fCalibList;

    double fLastBiasVoltageLo = -999.;
    double fLastBiasVoltageHi = -999.;

    QString getCalibParameter(const QString& name);
    bool currentCalibValid();
};

#endif // PARAMETERMONITORFORM_H
