#include "scanform.h"
#include "ui_scanform.h"
#include <QFileDialog>
#include <QPointF>
#include <QThread>
#include <QVector>
#include <cmath>
#include <limits>
#include <muondetector_structs.h>
#include <qwt_symbol.h>

ScanForm::ScanForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ScanForm)
{
    ui->setupUi(this);
    ui->scanParComboBox->clear();
    foreach (QString item, SP_NAMES) {
        ui->scanParComboBox->addItem(item);
    }
    ui->observableComboBox->clear();
    foreach (QString item, OP_NAMES) {
        ui->observableComboBox->addItem(item);
    }
    ui->minRangeLineEdit->setValidator(new QDoubleValidator(.001, 3.3, 4, ui->minRangeLineEdit));
    ui->maxRangeLineEdit->setValidator(new QDoubleValidator(.001, 3.3, 4, ui->maxRangeLineEdit));
    ui->stepSizeLineEdit->setValidator(new QDoubleValidator(0., 1.0, 5, ui->stepSizeLineEdit));

    ui->scanPlot->setTitle("Parameter Scan");
    ui->scanPlot->setAxisTitle(QwtPlot::xBottom, "scanpar");
    ui->scanPlot->setAxisTitle(QwtPlot::yLeft, "observable");

    ui->scanPlot->addCurve("parscan", Qt::blue);
    ui->scanPlot->curve("parscan").setStyle(QwtPlotCurve::NoCurve);
    QwtSymbol* sym = new QwtSymbol(QwtSymbol::Rect, QBrush(Qt::blue, Qt::SolidPattern), QPen(Qt::black), QSize(5, 5));
    ui->scanPlot->curve("parscan").setSymbol(sym);

    ui->scanPlot->setAxisAutoScale(QwtPlot::xBottom, true);
    ui->scanPlot->setAxisAutoScale(QwtPlot::yLeft, true);
    //ui->scanPlot->replot();
    ui->scanPlot->setEnabled(false);

    connect(ui->exportDataPushButton, &QPushButton::clicked, this, &ScanForm::exportData);
    ui->exportDataPushButton->setEnabled(false);
    ui->currentScanGroupBox->setEnabled(false);
    ui->scanProgressBar->reset();
}

ScanForm::~ScanForm()
{
    delete ui;
}

void ScanForm::onDacReadbackReceived(uint8_t channel, float value)
{
    if (channel > 3 || active)
        return;
    fLastDacs[channel] = value;
}

void ScanForm::onTimeMarkReceived(const UbxTimeMarkStruct& tm)
{
    if (!tm.risingValid) {
        qDebug() << "received invalid timemark";
        return;
    }
    if (active && OP_NAMES[obsPar] == "UBXRATE") {
        if (waitForFirst) {
            currentCounts = 0;
            currentTimeInterval = 0.;
            waitForFirst = false;
        } else {
            currentCounts += (uint16_t)(tm.evtCounter - lastTM.evtCounter);
            double interval = (tm.rising.tv_nsec - lastTM.rising.tv_nsec) * 1e-9;
            interval += tm.rising.tv_sec - lastTM.rising.tv_sec;
            currentTimeInterval += interval;
            if ((currentTimeInterval > maxMeasurementTimeInterval)
                || ((currentCounts > maxMeasurementStatistics)
                    && (currentTimeInterval > 1))) {
                scanParIteration();
            }
        }
    }
    lastTM = tm;
}

void ScanForm::scanParIteration()
{
    if (!active)
        return;
    if (OP_NAMES[obsPar] == "UBXRATE") {
        double rate = currentCounts / currentTimeInterval;
        scanData[currentScanPar].value = rate;
        scanData[currentScanPar].error = std::sqrt(currentCounts) / currentTimeInterval;
        ui->currentScanparLabel->setText(QString::number(currentScanPar));
        ui->currentObservableLabel->setText(QString::number(rate) + " +- " + QString::number(scanData[currentScanPar].error));
        updateScanPlot();
    }
    currentScanPar += stepSize;
    if (currentScanPar > maxRange) {
        // measurement finished
        finishScan();
        return;
    }
    adjustScanPar(SP_NAMES[scanPar], currentScanPar);
    ui->scanProgressBar->setValue(std::lround((currentScanPar - minRange) / stepSize));
    waitForFirst = true;
}

void ScanForm::adjustScanPar(QString scanParName, double value)
{
    if (scanParName == "THR1") {
        emit setThresholdVoltage(0, value);
    } else if (scanParName == "THR2") {
        emit setThresholdVoltage(1, value);
    } else if (scanParName == "BIAS") {
        emit setBiasControlVoltage(value);
    }
    QThread::msleep(100);
}

void ScanForm::on_scanStartPushButton_clicked()
{
    if (active) {
        finishScan();
        return;
    }
    bool ok = false;
    minRange = ui->minRangeLineEdit->text().toDouble(&ok);
    if (!ok)
        return;
    maxRange = ui->maxRangeLineEdit->text().toDouble(&ok);
    if (!ok)
        return;
    stepSize = ui->stepSizeLineEdit->text().toDouble(&ok);
    if (!ok)
        return;
    scanPar = ui->scanParComboBox->currentIndex();
    if (SP_NAMES[scanPar] == "VOID")
        return;
    obsPar = ui->observableComboBox->currentIndex();
    if (OP_NAMES[obsPar] == "VOID")
        return;

    maxMeasurementTimeInterval = ui->timeIntervalSpinBox->value();
    if (ui->limitStatisticsCheckBox->isChecked()) {
        int power = ui->maxStatisticsComboBox->currentIndex() + 1;
        maxMeasurementStatistics = std::pow(10, power);
    } else
        maxMeasurementStatistics = std::numeric_limits<unsigned long>::max();

    if (OP_NAMES[obsPar] == "UBXRATE") {
        emit gpioInhibitChanged(true);
        emit mqttInhibitChanged(true);
    }
    // values seem to be valid
    // start the scan
    ui->scanStartPushButton->setText(tr("Stop Scan"));
    currentScanPar = minRange;
    adjustScanPar(SP_NAMES[scanPar], currentScanPar);

    active = true;
    waitForFirst = true;
    scanData.clear();
    ui->scanProgressBar->setRange(0, 1 + std::lround(std::abs(maxRange - minRange) / stepSize));
    ui->scanProgressBar->setValue(0);
    ui->currentScanGroupBox->setEnabled(true);
    ui->scanparNameLabel->setText(SP_NAMES[scanPar]);
    ui->observableNameLabel->setText(OP_NAMES[obsPar]);
    ui->currentScanparLabel->setText(QString::number(minRange));
    ui->currentObservableLabel->setText("---");
}

void ScanForm::finishScan()
{
    ui->scanStartPushButton->setText(tr("Start Scan"));
    if (scanPar > 0)
        adjustScanPar(SP_NAMES[scanPar], fLastDacs[scanPar - 1]);
    active = false;
    emit gpioInhibitChanged(false);
    emit mqttInhibitChanged(false);
    updateScanPlot();
    ui->exportDataPushButton->setEnabled(true);
    ui->currentScanGroupBox->setEnabled(false);
    ui->scanProgressBar->reset();
}

void ScanForm::updateScanPlot()
{
    QVector<QPointF> vec;
    for (auto it = scanData.constBegin(); it != scanData.constEnd(); ++it) {
        QPointF p1;
        if (!plotDifferential) {
            p1.rx() = it.key();
            p1.ry() = it.value().value;
        } else {
            if (it == --scanData.constEnd())
                continue;
            p1.rx() = it.key();
            auto it2 = it;
            ++it2;
            p1.ry() = it.value().value - it2.value().value;
            if (ui->scanPlot->getLogY() && p1.ry() <= 0.)
                continue;
        }
        vec.push_back(p1);
    }

    ui->scanPlot->curve("parscan").setSamples(vec);
    ui->scanPlot->replot();
}

void ScanForm::on_plotDifferentialCheckBox_toggled(bool checked)
{
    plotDifferential = checked;
    updateScanPlot();
}

void ScanForm::onUiEnabledStateChange(bool connected)
{
    if (connected) {
    } else {
        if (active)
            finishScan();
    }
    this->setEnabled(connected);
    ui->scanPlot->setEnabled(connected);
}

void ScanForm::exportData()
{
    QString types("ASCII raw data (*.txt)");
    QString filter; // Type of filter
    QString txtExt = ".txt";
    QString suggestedName = "";
    QString fn = QFileDialog::getSaveFileName(this, tr("Export Plot"),
        suggestedName, types, &filter);

    if (!fn.isEmpty()) { // If filename is not null
        if (fn.contains(txtExt)) {
            fn.remove(txtExt);
        }

        if (filter.contains(txtExt)) {
            fn += txtExt;
            // export histo in asci raw data format
            QFile file(fn);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return;
            QTextStream out(&file);

            out << "# Parameter Scan\n";
            out << "# scanparValue measValue measError diffMeasValue diffMeasError temp\n";
            for (auto it = scanData.constBegin(); it != scanData.constEnd(); ++it) {
                double x = it.key();
                ScanPoint point1 = it.value();
                ScanPoint point2 {};
                bool hasSuccessor { false };
                if (it != --scanData.constEnd()) {
                    hasSuccessor = true;
                    auto it2 = it;
                    ++it2;
                    point2 = it2.value();
                }
                out << QString::number(x, 'g', 10) << "  "
                    << QString::number(point1.value, 'g', 10) << "  "
                    << QString::number(point1.error, 'g', 10) << "  ";

                if (hasSuccessor) {
                    out << QString::number(point1.value - point2.value, 'g', 10) << "  "
                        << QString::number(point1.error + point2.error, 'g', 10) << "  ";
                } else {
                    out << "0 0 ";
                }
                out << QString::number(point1.temp, 'g', 10) << "\n";
            }
        }
    }
}
