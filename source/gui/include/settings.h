#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QTableWidget>
#include <QtWidgets>
#include <ublox_structs.h>

class GnssSatellite;

class UbxMsgRateTableItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    uint16_t key;
    int rate;
    QString name;
};

namespace Ui {
class Settings;
}

class Settings : public QDialog {
    Q_OBJECT
public:
    explicit Settings(QWidget* parent = nullptr);

signals:
    void sendSetUbxMsgRateChanges(QMap<uint16_t, int> ubxMsgRateChanges);
    void sendRequestUbxMsgRates();
    void sendUbxReset();
    void sendUbxConfigDefault();
    void setGnssConfigs(const QVector<GnssConfigStruct>& configList);
    void setTP5Config(const UbxTimePulseStruct& tp);
    void sendUbxSaveCfg();

public slots:
    void addUbxMsgRates(QMap<uint16_t, int> ubxMsgRates);
    void onItemChanged(QTableWidgetItem* item);
    void onUiEnabledStateChange(bool connected);
    void onTxBufReceived(quint8 val);
    void onTxBufPeakReceived(quint8 val);
    void onRxBufReceived(quint8 val);
    void onRxBufPeakReceived(quint8 val);
    void onGnssConfigsReceived(quint8 numTrkCh, const QVector<GnssConfigStruct>& configList);
    void onTP5Received(const UbxTimePulseStruct& tp);
    void onConfigChanged();

private slots:
    void onSettingsButtonBoxClicked(QAbstractButton* button);
    void on_ubxResetPushButton_clicked();
    void writeGnssConfig();
    void writeTpConfig();

    void on_timeGridComboBox_currentIndexChanged(int index);

    void on_freqPeriodLineEdit_editingFinished();

    void on_freqPeriodLockLineEdit_editingFinished();

    void on_pulseLenLineEdit_editingFinished();

    void on_pulseLenLockLineEdit_editingFinished();

    void on_antDelayLineEdit_editingFinished();

    void on_groupDelayLineEdit_editingFinished();

    void on_userDelayLineEdit_editingFinished();

    void on_saveConfigPushButton_clicked();

private:
    Ui::Settings* ui;
    QMap<uint16_t, int> oldSettings;
    bool fGnssConfigChanged = false;
    bool fTpConfigChanged = false;
    UbxTimePulseStruct fTpConfig;
};

#endif // SETTINGS_H
