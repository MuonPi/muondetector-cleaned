#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QErrorMessage>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTime>
#include <QVector>
#include <gpio_pin_definitions.h>
#include <tcpconnection.h>

// for sig handling:
#include <sys/types.h>

struct I2cDeviceEntry;
struct CalibStruct;
struct GeodeticPos;
struct GnssConfigStruct;
class GnssSatellite;
class CalibForm;
class CalibScanDialog;
struct UbxTimePulseStruct;
class Histogram;
struct GnssMonHwStruct;
struct GnssMonHw2Struct;
struct LogInfoStruct;
struct UbxTimeMarkStruct;

enum class TCP_MSG_KEY : quint16;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

signals:
    void addUbxMsgRates(QMap<uint16_t, int> ubxMsgRates);
    void sendTcpMessage(TcpMessage tcpMessage);
    void closeConnection();
    void gpioRates(quint8 whichrate, QVector<QPointF> rate);
    void tcpDisconnected();
    void setUiEnabledStates(bool enabled);
    void geodeticPos(const GeodeticPos& pos);
    void adcSampleReceived(uint8_t channel, float value);
    void adcTraceReceived(const QVector<float>& sampleBuffer);
    void inputSwitchReceived(uint8_t);
    void dacReadbackReceived(uint8_t channel, float value);
    void biasSwitchReceived(bool state);
    void preampSwitchReceived(uint8_t channel, bool state);
    void gainSwitchReceived(bool state);
    void temperatureReceived(float temp);
    void i2cStatsReceived(quint32 bytesRead, quint32 bytesWritten, const QVector<I2cDeviceEntry>& deviceList);
    void spiStatsReceived(bool spiPresent);
    void calibReceived(bool valid, bool eepromValid, quint64 id, const QVector<CalibStruct>& calibList);
    void satsReceived(const QVector<GnssSatellite>& satList);
    void gnssConfigsReceived(quint8 numTrkCh, const QVector<GnssConfigStruct>& configList);
    void timeAccReceived(quint32 acc);
    void freqAccReceived(quint32 acc);
    void intCounterReceived(quint32 cnt);
    void txBufReceived(quint8 val);
    void txBufPeakReceived(quint8 val);
    void rxBufReceived(quint8 val);
    void rxBufPeakReceived(quint8 val);
    void gpsMonHWReceived(const GnssMonHwStruct& hwstruct);
    void gpsMonHW2Received(const GnssMonHw2Struct& hw2struct);
    void gpsVersionReceived(const QString& swString, const QString& hwString, const QString& protString);
    void gpsFixReceived(quint8 val);
    void ubxUptimeReceived(quint32 val);
    void gpsTP5Received(const UbxTimePulseStruct& tp);
    void histogramReceived(const Histogram& h);
    void triggerSelectionReceived(GPIO_PIN signal);
    void timepulseReceived();
    void adcModeReceived(quint8 mode);
    void logInfoReceived(const LogInfoStruct& lis);
    void mqttStatusChanged(bool connected);
    void timeMarkReceived(const UbxTimeMarkStruct&);
    void polaritySwitchReceived(bool pol1, bool pol2);
    void gpioInhibitReceived(bool inhibit);
    void mqttInhibitReceived(bool inhibit);

public slots:
    void receivedTcpMessage(TcpMessage tcpMessage);
    void receivedGpioRisingEdge(GPIO_PIN pin);
    void sendRequestUbxMsgRates();
    void sendSetUbxMsgRateChanges(QMap<uint16_t, int> changes);
    void onSendUbxReset();
    void makeConnection(QString ipAddress, quint16 port);
    void onTriggerSelectionChanged(GPIO_PIN signal);
    void onHistogramCleared(QString histogramName);
    void onAdcModeChanged(quint8 mode);
    void onRateScanStart(uint8_t ch);
    void gpioInhibit(bool inhibit);
    void mqttInhibit(bool inhibit);
    void onPolarityChanged(bool pol1, bool pol2);

private slots:
    void resetAndHit();
    void resetXorHit();
    void sendRequestGpioRates();
    void sendRequestGpioRateBuffer();
    void sendValueUpdateRequests();
    void sendPreamp1Switch(bool status);
    void sendPreamp2Switch(bool status);
    void sendSetBiasStatus(bool status);

    void onIpButtonClicked();
    void connected();
    void connection_error(int error_code, const QString message);
    void sendInputSwitch(uint8_t id);

    void on_discr1Save_clicked();
    void on_discr2Save_clicked();

    void on_biasPowerButton_clicked();
    void on_biasVoltageSlider_sliderReleased();
    void on_biasVoltageSlider_valueChanged(int value);
    void on_biasVoltageSlider_sliderPressed();
    void onCalibUpdated(const QVector<CalibStruct>& items);
    void on_biasControlTypeComboBox_currentIndexChanged(int index);
    void onSetGnssConfigs(const QVector<GnssConfigStruct>& configList);
    void onSetTP5Config(const UbxTimePulseStruct& tp);
    void on_biasVoltageDoubleSpinBox_valueChanged(double arg1);
    void on_saveDacButton_clicked();

private:
    Ui::MainWindow* ui;
    void uiSetConnectedState();
    void uiSetDisconnectedState();
    float parseValue(QString text);
    // ToDo: remove the sendRequest(quint) functions when switch-over to TCP_MSG_KEY is completed
    void sendRequest(quint16 requestSig);
    void sendRequest(TCP_MSG_KEY requestSig);
    void sendRequest(quint16 requestSig, quint8 par);
    void sendRequest(TCP_MSG_KEY requestSig, quint8 par);
    void sendSetBiasVoltage(float voltage);
    void sendSetThresh(uint8_t channel, float value);
    void setMaxThreshVoltage(float voltage);
    void sendGainSwitch(bool status);

    void updateUiProperties();
    int verbose = 0;
    float biasDacVoltage = 0.;
    bool biasON, uiValuesUpToDate = false;
    quint8 pcaPortMask = 0;
    std::array<double, 2> sliderValues { 0, 0 };
    bool sliderValuesDirty { true };
    float maxThreshVoltage;
    QErrorMessage errorM;
    TcpConnection* tcpConnection = nullptr;
    QStandardItemModel* addresses;
    QList<QStandardItem*>* addressColumn;
    bool saveSettings(QStandardItemModel* model);
    bool loadSettings(QStandardItemModel* model);
    bool eventFilter(QObject* object, QEvent* event);
    bool connectedToDemon = false;
    bool mouseHold = false;
    bool automaticRatePoll = true;
    QTimer andTimer, xorTimer, ratePollTimer;
    CalibForm* calib = nullptr;
    CalibScanDialog* calibscandialog = nullptr;
    double biasCalibOffset = 0.;
    double biasCalibSlope = 1.;
    double minBiasVoltage = 0.;
    double maxBiasVoltage = 3.3;
};

#endif // MAINWINDOW_H
