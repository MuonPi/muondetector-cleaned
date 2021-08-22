#include <muondetector_structs.h>
#include <settings.h>
#include <ubx_msg_key_name_map.h>
#include <ui_settings.h>

Settings::Settings(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::Settings)
{
    ui->setupUi(this);
    ui->ubloxSignalStates->setColumnCount(2);
    ui->ubloxSignalStates->verticalHeader()->setVisible(false);
    ui->ubloxSignalStates->setShowGrid(false);
    ui->ubloxSignalStates->setAlternatingRowColors(true);
    ui->ubloxSignalStates->setHorizontalHeaderLabels(QStringList({ "Signature", "Update Rate" }));
    ui->ubloxSignalStates->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    connect(ui->settingsButtonBox, &QDialogButtonBox::clicked, this, &Settings::onSettingsButtonBoxClicked);
    connect(ui->ubloxSignalStates, &QTableWidget::itemChanged, this, &Settings::onItemChanged);
    ui->ubloxSignalStates->blockSignals(true);
    connect(ui->gnssConfigButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [this](int) {
        this->fGnssConfigChanged = true;
        this->onConfigChanged();
    });
    connect(ui->tpConfigButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [this](int) {
        this->fTpConfigChanged = true;
        this->onConfigChanged();
    });
    this->setDisabled(true);
    emit sendRequestUbxMsgRates();
}

void Settings::onConfigChanged()
{
    this->fTpConfigChanged = true;
    this->ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    this->ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setEnabled(true);
}

void Settings::onItemChanged(QTableWidgetItem* item)
{
    ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setEnabled(true);
    if (item->column() == 0) {
        return;
    }
    bool ok = false;
    if (item->text().toInt(&ok) != ((UbxMsgRateTableItem*)item)->rate) {
        if (!ok) {
            qDebug() << "rate is no integer";
        }
        ui->ubloxSignalStates->item(item->row(), 0)->setCheckState(Qt::Unchecked);
    } else {
        ui->ubloxSignalStates->item(item->row(), 0)->setCheckState(Qt::Checked);
    }
}

void Settings::addUbxMsgRates(QMap<uint16_t, int> ubxMsgRates)
{
    ui->ubloxSignalStates->clearContents();
    ui->ubloxSignalStates->setRowCount(0);
    ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
    ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(true);
    ui->ubloxSignalStates->blockSignals(true);
    oldSettings = ubxMsgRates;
    for (QMap<uint16_t, int>::iterator it = ubxMsgRates.begin(); it != ubxMsgRates.end(); it++) {
        UbxMsgRateTableItem* item = new UbxMsgRateTableItem();
        UbxMsgRateTableItem* value = new UbxMsgRateTableItem();
        item->setCheckState(Qt::CheckState::Checked);
        item->setFlags(item->flags() & (~Qt::ItemIsUserCheckable)); // disables checkbox edit from user
        item->setFlags(item->flags() & (~Qt::ItemIsEditable));
        item->key = it.key();
        item->name = ubxMsgKeyNameMap.value(it.key());
        item->setText(item->name);
        item->setSizeHint(QSize(120, 24));
        value->key = it.key();
        value->rate = it.value();
        value->setText(QString::number(value->rate).rightJustified(3, '0'));
        ui->ubloxSignalStates->insertRow(ui->ubloxSignalStates->rowCount());
        ui->ubloxSignalStates->setItem(ui->ubloxSignalStates->rowCount() - 1, 0, item);
        ui->ubloxSignalStates->setItem(ui->ubloxSignalStates->rowCount() - 1, 1, value);
        // I don't know why item gets (sometimes) uncheckd when transfered to the TableView
        // it seems to work now though:
        item->setCheckState(Qt::CheckState::Checked);
    }
    ui->ubloxSignalStates->blockSignals(false);
    ui->ubloxSignalStates->resizeColumnsToContents();
    ui->ubloxSignalStates->resizeRowsToContents();
    ui->ubloxSignalStates->setColumnWidth(0, 143);
    ui->ubloxSignalStates->setColumnWidth(1, 100);
}

void Settings::onSettingsButtonBoxClicked(QAbstractButton* button)
{
    if (button == ui->settingsButtonBox->button(QDialogButtonBox::Apply)) {
        ui->ubloxSignalStates->blockSignals(true);
        QMap<uint16_t, int> changedSettings;
        for (int i = 0; i < ui->ubloxSignalStates->rowCount(); i++) {
            UbxMsgRateTableItem* item = (UbxMsgRateTableItem*)ui->ubloxSignalStates->item(i, 0);
            if (item->checkState() == Qt::Checked) {
                continue;
            }
            UbxMsgRateTableItem* value = (UbxMsgRateTableItem*)ui->ubloxSignalStates->item(i, 1);
            bool ok = false;
            int newRate = value->text().toInt(&ok);
            if (ok && value->rate != newRate) {
                changedSettings.insert(value->key, newRate);
            }
        }
        if (changedSettings.size() > 0) {
            emit sendSetUbxMsgRateChanges(changedSettings);
        } else if (fGnssConfigChanged) {
            fGnssConfigChanged = false;
            writeGnssConfig();
        } else if (fTpConfigChanged) {
            fTpConfigChanged = false;
            writeTpConfig();
        }
        ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
        ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(true);
        ui->ubloxSignalStates->blockSignals(false);
    }
    if (button == ui->settingsButtonBox->button(QDialogButtonBox::Discard)) {
        onTP5Received(fTpConfig);
        ui->ubloxSignalStates->blockSignals(true);
        emit sendRequestUbxMsgRates();
        ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
        ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(true);
    }
    if (button == ui->settingsButtonBox->button(QDialogButtonBox::RestoreDefaults)) {
        emit sendUbxConfigDefault();
    }
}

void Settings::onUiEnabledStateChange(bool connected)
{
    if (connected) {
        this->setEnabled(true);
        ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(true);
        ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
        ui->ubloxSignalStates->blockSignals(false);
    } else {
        ui->ubloxSignalStates->clearContents();
        ui->ubloxSignalStates->setRowCount(0);
        ui->ubloxSignalStates->blockSignals(true);
        ui->gnssConfigButtonGroup->blockSignals(true);
        ui->gnssGpsCheckBox->setEnabled(false);
        ui->gnssGpsCheckBox->setChecked(false);
        ui->gnssSbasCheckBox->setEnabled(false);
        ui->gnssSbasCheckBox->setChecked(false);
        ui->gnssGalCheckBox->setEnabled(false);
        ui->gnssGalCheckBox->setChecked(false);
        ui->gnssBeidCheckBox->setEnabled(false);
        ui->gnssBeidCheckBox->setChecked(false);
        ui->gnssQzssCheckBox->setEnabled(false);
        ui->gnssQzssCheckBox->setChecked(false);
        ui->gnssGlnsCheckBox->setEnabled(false);
        ui->gnssGlnsCheckBox->setChecked(false);
        ui->gnssImesCheckBox->setEnabled(false);
        ui->gnssImesCheckBox->setChecked(false);
        ui->numTrkChannelsLabel->setText("N/A");
        ui->gnssConfigButtonGroup->blockSignals(true);
        ui->rxBufProgressBar->setValue(0);
        ui->txBufProgressBar->setValue(0);
        ui->rxPeakLabel->setText("max:");
        ui->txPeakLabel->setText("max:");
        this->setDisabled(true);
    }
}

void Settings::onTxBufReceived(quint8 val)
{
    ui->txBufProgressBar->setValue(val);
}

void Settings::onTxBufPeakReceived(quint8 val)
{
    ui->txPeakLabel->setText("max: " + QString::number(val) + "%");
}

void Settings::onRxBufReceived(quint8 val)
{
    ui->rxBufProgressBar->setValue(val);
}

void Settings::onRxBufPeakReceived(quint8 val)
{
    ui->rxPeakLabel->setText("max: " + QString::number(val) + "%");
}

void Settings::onGnssConfigsReceived(quint8 numTrkCh, const QVector<GnssConfigStruct>& configList)
{
    ui->gnssConfigButtonGroup->blockSignals(true);
    ui->numTrkChannelsLabel->setText(QString::number(numTrkCh));
    for (int i = 0; i < configList.size(); i++) {
        if (configList[i].gnssId == 0) { // GPS
            ui->gnssGpsCheckBox->setEnabled(true);
            ui->gnssGpsCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 1) { // SBAS
            ui->gnssSbasCheckBox->setEnabled(true);
            ui->gnssSbasCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 2) { // GAL
            ui->gnssGalCheckBox->setEnabled(true);
            ui->gnssGalCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 3) { // BEID
            ui->gnssBeidCheckBox->setEnabled(true);
            ui->gnssBeidCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 4) { // IMES
            ui->gnssImesCheckBox->setEnabled(true);
            ui->gnssImesCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 5) { // QZSS
            ui->gnssQzssCheckBox->setEnabled(true);
            ui->gnssQzssCheckBox->setChecked(configList[i].flags & 0x01);
        } else if (configList[i].gnssId == 6) { // GLNS
            ui->gnssGlnsCheckBox->setEnabled(true);
            ui->gnssGlnsCheckBox->setChecked(configList[i].flags & 0x01);
        }
    }
    ui->gnssConfigButtonGroup->blockSignals(false);
    fGnssConfigChanged = false;
    ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(true);
    ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(true);
}

void Settings::onTP5Received(const UbxTimePulseStruct& tp)
{
    fTpConfig = tp;
    ui->tpConfigButtonGroup->blockSignals(true);
    ui->syncedPulseGroupBox->blockSignals(true);
    ui->unsyncedPulseGroupBox->blockSignals(true);
    ui->antDelayLineEdit->setText(QString::number(tp.antCableDelay));
    ui->groupDelayLineEdit->setText(QString::number(tp.rfGroupDelay));
    ui->userDelayLineEdit->setText(QString::number(tp.userConfigDelay));
    ui->freqPeriodLineEdit->setText(QString::number(tp.freqPeriod));
    ui->freqPeriodLockLineEdit->setText(QString::number(tp.freqPeriodLock));
    ui->pulseLenLineEdit->setText(QString::number(tp.pulseLenRatio));
    ui->pulseLenLockLineEdit->setText(QString::number(tp.pulseLenRatioLock));
    ui->tpActiveCheckBox->setChecked(tp.flags & UbxTimePulseStruct::ACTIVE);

    ui->lockGpsCheckBox->setChecked(tp.flags & UbxTimePulseStruct::LOCK_GPS);

    ui->lockOtherCheckBox->setChecked(tp.flags & UbxTimePulseStruct::LOCK_OTHER);

    ui->timeGridComboBox->setCurrentIndex((tp.flags & UbxTimePulseStruct::GRID_UTC_GPS) >> 7);
    ui->tpPolarityCheckBox->setChecked((tp.flags & UbxTimePulseStruct::POLARITY));
    ui->tpAlignTowCheckBox->setChecked((tp.flags & UbxTimePulseStruct::ALIGN_TO_TOW));
    ui->tpConfigButtonGroup->blockSignals(false);
    ui->syncedPulseGroupBox->blockSignals(false);
    ui->unsyncedPulseGroupBox->blockSignals(false);
}

void Settings::on_ubxResetPushButton_clicked()
{
    // reset Ublox device
    emit sendUbxReset();
}

void Settings::writeGnssConfig()
{
    QVector<GnssConfigStruct> configList;
    if (ui->gnssGpsCheckBox->isEnabled()) { // GPS
        GnssConfigStruct config;
        config.gnssId = 0;
        config.maxTrkCh = 14;
        config.resTrkCh = 8;
        config.flags = (ui->gnssGpsCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssSbasCheckBox->isEnabled()) { // SBAS
        GnssConfigStruct config;
        config.gnssId = 1;
        config.maxTrkCh = 3;
        config.resTrkCh = 1;
        config.flags = (ui->gnssSbasCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssGalCheckBox->isEnabled()) { // GAL
        GnssConfigStruct config;
        config.gnssId = 2;
        config.maxTrkCh = 8;
        config.resTrkCh = 4;
        config.flags = (ui->gnssGalCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssBeidCheckBox->isEnabled()) { // BEID
        GnssConfigStruct config;
        config.gnssId = 3;
        config.maxTrkCh = 10;
        config.resTrkCh = 4;
        config.flags = (ui->gnssBeidCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssImesCheckBox->isEnabled()) { // IMES
        GnssConfigStruct config;
        config.gnssId = 4;
        config.maxTrkCh = 8;
        config.resTrkCh = 0;
        config.flags = (ui->gnssImesCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssQzssCheckBox->isEnabled()) { // QZSS
        GnssConfigStruct config;
        config.gnssId = 5;
        config.maxTrkCh = 3;
        config.resTrkCh = 0;
        config.flags = (ui->gnssQzssCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (ui->gnssGlnsCheckBox->isEnabled()) { // GLNS
        GnssConfigStruct config;
        config.gnssId = 6;
        config.maxTrkCh = 12;
        config.resTrkCh = 6;
        config.flags = (ui->gnssGlnsCheckBox->isChecked()) ? 0x00000001 : 0;
        configList.push_back(config);
    }
    if (configList.size())
        emit setGnssConfigs(configList);
}

void Settings::writeTpConfig()
{
    UbxTimePulseStruct tp;
    tp.tpIndex = 0;
    bool ok = false;
    tp.antCableDelay = ui->antDelayLineEdit->text().toInt(&ok);
    if (!ok)
        return;
    tp.rfGroupDelay = ui->groupDelayLineEdit->text().toInt(&ok);
    if (!ok)
        return;
    tp.userConfigDelay = ui->userDelayLineEdit->text().toLong(&ok);
    if (!ok)
        return;
    tp.freqPeriod = ui->freqPeriodLineEdit->text().toLong(&ok);
    if (!ok)
        return;
    tp.freqPeriodLock = ui->freqPeriodLockLineEdit->text().toLong(&ok);
    if (!ok)
        return;
    tp.pulseLenRatio = ui->pulseLenLineEdit->text().toLong(&ok);
    if (!ok)
        return;
    tp.pulseLenRatioLock = ui->pulseLenLockLineEdit->text().toLong(&ok);
    if (!ok)
        return;
    tp.flags = UbxTimePulseStruct::IS_LENGTH;
    if (ui->tpActiveCheckBox->isChecked())
        tp.flags |= UbxTimePulseStruct::ACTIVE;
    if (ui->tpPolarityCheckBox->isChecked())
        tp.flags |= UbxTimePulseStruct::POLARITY;

    if (ui->lockGpsCheckBox->isChecked())
        tp.flags |= UbxTimePulseStruct::LOCK_GPS;

    if (ui->lockOtherCheckBox->isChecked())
        tp.flags |= UbxTimePulseStruct::LOCK_OTHER;

    if (ui->tpAlignTowCheckBox->isChecked())
        tp.flags |= UbxTimePulseStruct::ALIGN_TO_TOW;
    tp.flags |= ((ui->timeGridComboBox->currentIndex() << 7) & UbxTimePulseStruct::GRID_UTC_GPS);
    emit setTP5Config(tp);
}

void Settings::on_timeGridComboBox_currentIndexChanged(int /*index*/)
{
    fTpConfigChanged = true;
    ui->settingsButtonBox->button(QDialogButtonBox::Apply)->setDisabled(false);
    ui->settingsButtonBox->button(QDialogButtonBox::Discard)->setDisabled(false);
}

void Settings::on_freqPeriodLineEdit_editingFinished()
{
    bool ok = false;
    ui->freqPeriodLineEdit->text().toLong(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_freqPeriodLockLineEdit_editingFinished()
{
    bool ok = false;
    ui->freqPeriodLockLineEdit->text().toLong(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_pulseLenLineEdit_editingFinished()
{
    bool ok = false;
    ui->pulseLenLineEdit->text().toLong(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_pulseLenLockLineEdit_editingFinished()
{
    bool ok = false;
    ui->pulseLenLockLineEdit->text().toLong(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_antDelayLineEdit_editingFinished()
{
    bool ok = false;
    ui->antDelayLineEdit->text().toInt(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_groupDelayLineEdit_editingFinished()
{
    bool ok = false;
    ui->groupDelayLineEdit->text().toInt(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_userDelayLineEdit_editingFinished()
{
    bool ok = false;
    ui->userDelayLineEdit->text().toLong(&ok);
    if (!ok)
        onTP5Received(fTpConfig);
    else {
        fTpConfigChanged = true;
        this->onConfigChanged();
    }
}

void Settings::on_saveConfigPushButton_clicked()
{
    // save config
    emit sendUbxSaveCfg();
}
