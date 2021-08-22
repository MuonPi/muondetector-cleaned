#include "calibscandialog.h"
#include "ui_calibscandialog.h"
#include <QMessageBox>
#include <QThread>
#include <calibform.h>
#include <muondetector_structs.h>

#define calVoltMin 0.3
#define calVoltMax 2.5

#define BIAS_SCAN_INCREMENT 0.025 // scan step in Volts

inline static double sqr(double x)
{
    return x * x;
}

/*
 linear regression algorithm
 taken from:
   http://stackoverflow.com/questions/5083465/fast-efficient-least-squares-fit-algorithm-in-c
 parameters:
  n = number of data points
  xarray,yarray  = arrays of data
  *offs = output intercept
  *slope  = output slope
 */
bool calcLinearCoefficients(const QVector<QPointF>& points, double* offs, double* slope)
{
    int n = points.size();
    if (n < 3)
        return false;

    double sumx = 0.0; /* sum of x                      */
    double sumx2 = 0.0; /* sum of x**2                   */
    double sumxy = 0.0; /* sum of x * y                  */
    double sumy = 0.0; /* sum of y                      */
    double sumy2 = 0.0; /* sum of y**2                   */

    double offsx = 0;
    double offsy = 0;

    for (int i = 0; i < n; i++) {
        sumx += points[i].x() - offsx;
        sumx2 += sqr(points[i].x() - offsx);
        sumxy += (points[i].x() - offsx) * (points[i].y() - offsy);
        sumy += (points[i].y() - offsy);
        sumy2 += sqr(points[i].y() - offsy);
    }

    double denom = (n * sumx2 - sqr(sumx));
    if (std::abs(denom) <= std::numeric_limits<double>::epsilon()) {
        // singular matrix. can't solve the problem.
        *slope = 0;
        *offs = 0;
        return false;
    }

    double m = (n * sumxy - sumx * sumy) / denom;
    double b = (sumy * sumx2 - sumx * sumxy) / denom;

    *slope = m;
    *offs = b + offsy;
    return true;
}

CalibScanDialog::CalibScanDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CalibScanDialog)
{
    ui->setupUi(this);
    connect(ui->startCurrentCalibPushButton, &QPushButton::clicked, this, &CalibScanDialog::startManualCurrentCalib);
    connect(ui->transferCurrentBiasPushButton, &QPushButton::clicked, this, &CalibScanDialog::transferCurrentCalibCoeffs);
    ui->transferCurrentBiasPushButton->setEnabled(false);
    ui->currentCalibProgressBar->setEnabled(false);
    ui->currentCalibProgressBar->setMaximum(5);
}

CalibScanDialog::~CalibScanDialog()
{
    delete ui;
}

void CalibScanDialog::onCalibReceived(bool /*valid*/, bool eepromValid, quint64 /*id*/, const QVector<CalibStruct>& calibList)
{
    if (!eepromValid)
        return;

    fCalibList.clear();
    for (int i = 0; i < calibList.size(); i++) {
        fCalibList.push_back(calibList[i]);
    }
    fSlope1 = getCalibParameter("COEFF1").toDouble();
    fOffs1 = getCalibParameter("COEFF0").toDouble();
    fSlope2 = getCalibParameter("COEFF3").toDouble();
    fOffs2 = getCalibParameter("COEFF2").toDouble();
}

void CalibScanDialog::onAdcSampleReceived(uint8_t channel, double value)
{
    double ubias = 0.;
    if (channel >= 2) {
        QString result = getCalibParameter("VDIV");
        if (result != "") {
            double vdiv = result.toDouble(nullptr);
            ubias = vdiv * value * 0.01;
        }
    }
    if (channel == 3) {
        double vdiv = getCalibParameter("VDIV").toDouble() * 0.01;
        double rsense = getCalibParameter("RSENSE").toDouble() * 0.1 * 0.001; // RSense in MOhm
        double ibias = vdiv * (fLastRSenseHiVoltage - value) / rsense;
        if (fCurrentCalibRunning)
            manualCurrentCalibProgress(ubias, ibias);
        fLastRSenseLoVoltage = value;
    } else if (channel == 2) {
        fLastRSenseHiVoltage = value;
    }
}

void CalibScanDialog::startManualCurrentCalib()
{
    if (fCurrentCalibRunning) {
        fCurrentCalibRunning = 0;
        return;
    }
    fCurrentCalibRunning = 1;
    ui->currentCalibProgressBar->setEnabled(true);
    ui->currentCalibProgressBar->setValue(fCurrentCalibRunning);
    QMessageBox msgBox;
    msgBox.setText("Disconnect the bias voltage");
    msgBox.setInformativeText("Disconnect the cable at the bias connector of the MuonPi PCB!");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Cancel) {
        fCurrentCalibRunning = 0;
        ui->currentCalibProgressBar->setValue(fCurrentCalibRunning);
        ui->currentCalibProgressBar->setEnabled(false);
        return;
    }
    fCurrentCalibRunning = 2;
    emit dynamic_cast<CalibForm*>(parent())->setBiasSwitch(false);
    QThread::msleep(500);
}

void CalibScanDialog::manualCurrentCalibProgress(double vbias, double ibias)
{
    static double currentMeasurements[3] = { 0., 0., 0. };

    ui->currentCalibProgressBar->setValue(fCurrentCalibRunning);
    if (fCurrentCalibRunning == 2) {
        currentMeasurements[0] = ibias;
        emit dynamic_cast<CalibForm*>(parent())->setBiasSwitch(true);
        QThread::msleep(500);
        fCurrentCalibRunning++;
    } else if (fCurrentCalibRunning == 3) {
        currentMeasurements[1] = ibias;
        fCurrentCalibRunning++;
        QMessageBox msgBox;
        msgBox.setText("Reconnect the bias voltage");
        msgBox.setInformativeText("Connect the cable again to the bias connector of the MuonPi PCB!");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        if (ret == QMessageBox::Cancel) {
            fCurrentCalibRunning = 0;
            ui->currentCalibProgressBar->setValue(fCurrentCalibRunning);
            ui->currentCalibProgressBar->setEnabled(false);
            return;
        }
        QThread::msleep(500);
        fCurrentCalibRunning++;
    } else if (fCurrentCalibRunning == 5) {
        currentMeasurements[2] = ibias;
        fCurrentCalibRunning = 0;
        fOffs2 = currentMeasurements[0];
        fSlope2 = (currentMeasurements[1] - currentMeasurements[2]) / vbias;
        ui->currentCalibOffsetLineEdit->setText(QString::number(fOffs2, 'g', 4));
        ui->currentCalibSlopeLineEdit->setText(QString::number(fSlope2, 'g', 4));
        ui->transferCurrentBiasPushButton->setEnabled(true);
        ui->currentCalibProgressBar->setValue(fCurrentCalibRunning);
        ui->currentCalibProgressBar->setEnabled(false);
    }
}

void CalibScanDialog::transferCurrentCalibCoeffs()
{
    emit dynamic_cast<CalibForm*>(parent())->setCalibParameter("COEFF2", QString::number(fOffs2));
    emit dynamic_cast<CalibForm*>(parent())->setCalibParameter("COEFF3", QString::number(fSlope2));
    uint8_t flags = getCalibParameter("CALIB_FLAGS").toUInt();
    flags |= CalibStruct::CalibStruct::CALIBFLAGS_CURRENT_COEFFS;
    emit dynamic_cast<CalibForm*>(parent())->setCalibParameter("CALIB_FLAGS", QString::number(flags));
    emit dynamic_cast<CalibForm*>(parent())->updateCalibTable();
    ui->transferCurrentBiasPushButton->setEnabled(false);
}

void CalibScanDialog::setCalibParameter(const QString& name, const QString& value)
{
    if (!fCalibList.empty()) {
        auto result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s) { return s.name == name.toStdString(); });
        if (result != fCalibList.end()) {
            result->value = value.toStdString();
        }
    }
}

QString CalibScanDialog::getCalibParameter(const QString& name)
{
    if (!fCalibList.empty()) {
        auto result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s) { return s.name == name.toStdString(); });
        if (result != fCalibList.end()) {
            return QString::fromStdString(result->value);
        }
    }
    return "";
}
