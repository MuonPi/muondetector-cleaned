#include "calibform.h"
#include "calibscandialog.h"
#include "ui_calibform.h"
#include <QThread>
#include <muondetector_structs.h>
#include <qwt_symbol.h>
#include <string>

using namespace std;

const static CalibStruct invalidCalibItem;

CalibForm::CalibForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CalibForm)
{
    ui->setupUi(this);
    ui->calibItemTableWidget->resizeColumnsToContents();

    calScan = new CalibScanDialog(this);
    calScan->setWindowTitle("Calibration Scan");
    calScan->hide();

    connect(ui->calibrationScanPushButton, &QPushButton::clicked, this, [this]() { this->calScan->show(); });
}

CalibForm::~CalibForm()
{
    delete ui;
}

void CalibForm::onCalibReceived(bool valid, bool eepromValid, quint64 id, const QVector<CalibStruct>& calibList)
{
    calScan->onCalibReceived(valid, eepromValid, id, calibList);

    QString str = "invalid";
    if (eepromValid)
        str = "valid";
    ui->eepromValidLabel->setText(str);
    str = "invalid";
    if (valid)
        str = "valid";
    ui->calibValidLabel->setText(str);
    ui->idLineEdit->setText(QString::number(id, 16));

    fCalibList.clear();
    for (int i = 0; i < calibList.size(); i++) {
        fCalibList.push_back(calibList[i]);
    }

    int ver = getCalibParameter("VERSION").toInt();
    ui->eepromHwVersionSpinBox->setValue(ver);
    double rsense = 0.1 * getCalibParameter("RSENSE").toInt();
    ui->rsenseDoubleSpinBox->setValue(rsense);
    double vdiv = 0.01 * getCalibParameter("VDIV").toInt();
    ui->vdivDoubleSpinBox->setValue(vdiv);
    int eepCycles = getCalibParameter("WRITE_CYCLES").toInt();
    ui->eepromWriteCyclesLabel->setText(QString::number(eepCycles));
    int featureFlags = getCalibParameter("FEATURE_FLAGS").toInt();
    ui->featureGnssCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_GNSS);
    ui->featureEnergyCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_ENERGY);
    ui->featureDetBiasCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_DETBIAS);
    ui->featurePreampBiasCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_PREAMP_BIAS);

    if (voltageCalibValid()) {
        fSlope1 = getCalibParameter("COEFF1").toDouble();
        fOffs1 = getCalibParameter("COEFF0").toDouble();
    }
    if (currentCalibValid()) {
        fSlope2 = getCalibParameter("COEFF3").toDouble();
        fOffs2 = getCalibParameter("COEFF2").toDouble();
    }
    updateCalibTable();
    QVector<CalibStruct> emptyList;
    emit updatedCalib(emptyList);
}

void CalibForm::updateCalibTable()
{
    ui->calibItemTableWidget->setRowCount(fCalibList.size());
    for (int i = 0; i < fCalibList.size(); i++) {
        QTableWidgetItem* newItem1 = new QTableWidgetItem(QString::fromStdString(fCalibList[i].name));
        newItem1->setSizeHint(QSize(90, 20));
        ui->calibItemTableWidget->setItem(i, 0, newItem1);
        QString type = QString::fromStdString(fCalibList[i].type);
        QString numberstr = "";
        if (type == "FLOAT") {
            double val = QString::fromStdString(fCalibList[i].value).toDouble(nullptr);
            numberstr = QString::number(val);
        } else {
            numberstr = QString::fromStdString(fCalibList[i].value);
        }

        QTableWidgetItem* newItem2 = new QTableWidgetItem(type);
        newItem2->setSizeHint(QSize(60, 20));
        ui->calibItemTableWidget->setItem(i, 1, newItem2);
        QTableWidgetItem* newItem3 = new QTableWidgetItem(numberstr);
        newItem3->setSizeHint(QSize(60, 20));
        ui->calibItemTableWidget->setItem(i, 2, newItem3);
        QTableWidgetItem* newItem4 = new QTableWidgetItem("0x" + QString("%1").arg(fCalibList[i].address, 2, 16, QChar('0')));
        newItem4->setSizeHint(QSize(40, 20));
        ui->calibItemTableWidget->setItem(i, 3, newItem4);
    }
    ui->calibItemTableWidget->resizeColumnsToContents();
    ui->calibItemTableWidget->resizeRowsToContents();
}

void CalibForm::onAdcSampleReceived(uint8_t channel, float value)
{
    calScan->onAdcSampleReceived(channel, value);
}

void CalibForm::on_readCalibPushButton_clicked()
{
    // calib reread triggered
    emit calibRequest();
}

void CalibForm::on_writeEepromPushButton_clicked()
{
    // write eeprom clicked
    emit updatedCalib(fCalibList);
    emit writeCalibToEeprom();
}

void CalibForm::doFit()
{
}

void CalibForm::setCalibParameter(const QString& name, const QString& value)
{
    if (!fCalibList.empty()) {
        auto result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s) { return s.name == name.toStdString(); });
        if (result != fCalibList.end()) {
            result->value = value.toStdString();
        }
    }
}

QString CalibForm::getCalibParameter(const QString& name)
{
    if (!fCalibList.empty()) {
        auto result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s) { return s.name == name.toStdString(); });
        if (result != fCalibList.end()) {
            return QString::fromStdString(result->value);
        }
    }
    return "";
}

const CalibStruct& CalibForm::getCalibItem(const QString& name)
{

    if (!fCalibList.empty()) {
        QVector<CalibStruct>::iterator result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s) { return s.name == name.toStdString(); });
        if (result != fCalibList.end()) {
            return *result;
        }
    }
    return invalidCalibItem;
}

bool CalibForm::voltageCalibValid()
{
    int calibFlags = getCalibParameter("CALIB_FLAGS").toUInt();
    if (calibFlags & CalibStruct::CALIBFLAGS_VOLTAGE_COEFFS)
        return true;
    return false;
}

bool CalibForm::currentCalibValid()
{
    int calibFlags = getCalibParameter("CALIB_FLAGS").toUInt();
    if (calibFlags & CalibStruct::CALIBFLAGS_CURRENT_COEFFS)
        return true;
    return false;
}

void CalibForm::onUiEnabledStateChange(bool connected)
{
    //measureBiasCalibGroupBox
    if (!connected) {
        ui->calibItemTableWidget->setRowCount(0);
        fCalibList.clear();
        ui->eepromValidLabel->setText("N/A");
        ui->calibValidLabel->setText("N/A");
        ui->idLineEdit->setText("N/A");
    }
    ui->calibItemsGroupBox->setEnabled(connected);
    ui->eepromGroupBox->setEnabled(connected);
    ui->calibrationScanPushButton->setEnabled(connected);
}

void CalibForm::on_calibItemTableWidget_cellChanged(int row, int column)
{
    if (column == 2) {
        QString name = ui->calibItemTableWidget->item(row, 0)->text();
        QString valstr = ui->calibItemTableWidget->item(row, 2)->text();
        if (valstr == "") {
            updateCalibTable();
            return;
        }
        bool ok = false;
        valstr.toDouble(&ok);
        if (!ok) {
            updateCalibTable();
            return;
        }
        setCalibParameter(name, valstr);
        QVector<CalibStruct> items;
        items.push_back(getCalibItem(name));
        emit updatedCalib(items);
    }
}
