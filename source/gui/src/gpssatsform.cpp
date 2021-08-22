#include "gpssatsform.h"
#include "ui_gpssatsform.h"
#include <QPainter>
#include <QPixmap>
#include <cmath>
#include <muondetector_structs.h>

const int MAX_IQTRACK_BUFFER = 250;

// helper function to format human readable numbers with common suffixes (k(ilo), M(ega), m(illi) etc.)
QString printReadableFloat(double value, int prec = 2, int lowOrderInhibit = -12, int highOrderInhibit = 9)
{
    QString str = "";
    QString suffix = "";
    double order = std::log10(std::fabs(value));
    if (order >= lowOrderInhibit && order <= highOrderInhibit) {
        if (order >= 9) {
            value *= 1e-9;
            suffix = "G";
        } else if (order >= 6) {
            value *= 1e-6;
            suffix = "M";
        } else if (order >= 3) {
            value *= 1e-3;
            suffix = "k";
        } else if (order >= 0) {
            suffix = "";
        } else if (order >= -3) {
            value *= 1000.;
            suffix = "m";
        } else if (order >= -6) {
            value *= 1e6;
            suffix = "u";
        } else if (order >= -9) {
            value *= 1e9;
            suffix = "n";
        } else if (order >= -12) {
            value *= 1e12;
            suffix = "p";
        }
    }
    char fmtChar = 'f';
    double newOrder = std::log10(std::fabs(value));
    if (fabs(newOrder) >= 3.) {
        fmtChar = 'g';
    } else {
        prec = prec - (int)newOrder - 1;
    }
    if (prec < 0)
        prec = 0;
    return QString::number(value, fmtChar, prec) + " " + suffix;
}

GpsSatsForm::GpsSatsForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::GpsSatsForm)
{
    ui->setupUi(this);
    ui->satsTableWidget->resizeColumnsToContents();
}

GpsSatsForm::~GpsSatsForm()
{
    delete ui;
}

void GpsSatsForm::onSatsReceived(const QVector<GnssSatellite>& satlist)
{
    ui->gnssPosWidget->onSatsReceived(satlist);

    QVector<GnssSatellite> newlist;
    QString str;
    QColor color;

    int nrGoodSats = 0;
    for (auto it = satlist.begin(); it != satlist.end(); it++)
        if (it->fCnr > 0)
            nrGoodSats++;

    ui->nrSatsLabel->setText(QString::number(nrGoodSats) + "/" + QString::number(satlist.size()));

    for (int i = 0; i < satlist.size(); i++) {
        if (ui->visibleSatsCheckBox->isChecked()) {
            if (satlist[i].fCnr > 0)
                newlist.push_back(satlist[i]);
        } else
            newlist.push_back(satlist[i]);
    }

    int N = newlist.size();
    ui->satsTableWidget->setRowCount(N);
    for (int i = 0; i < N; i++) {
        QTableWidgetItem* newItem1 = new QTableWidgetItem(QString::number(newlist[i].fSatId));
        newItem1->setSizeHint(QSize(25, 24));
        ui->satsTableWidget->setItem(i, 0, newItem1);
        QTableWidgetItem* newItem2 = new QTableWidgetItem(GNSS_ID_STRING[newlist[i].fGnssId]);
        newItem2->setSizeHint(QSize(50, 24));
        ui->satsTableWidget->setItem(i, 1, newItem2);
        QTableWidgetItem* newItem3 = new QTableWidgetItem(QString::number(newlist[i].fCnr));
        newItem3->setSizeHint(QSize(70, 24));
        ui->satsTableWidget->setItem(i, 2, newItem3);
        QTableWidgetItem* newItem4 = new QTableWidgetItem(QString::number(newlist[i].fAzim));
        newItem4->setSizeHint(QSize(100, 24));
        ui->satsTableWidget->setItem(i, 3, newItem4);
        QTableWidgetItem* newItem5 = new QTableWidgetItem(QString::number(newlist[i].fElev));
        newItem5->setSizeHint(QSize(100, 24));
        ui->satsTableWidget->setItem(i, 4, newItem5);
        QTableWidgetItem* newItem6 = new QTableWidgetItem(printReadableFloat(newlist[i].fPrRes, 2, 0));
        newItem6->setSizeHint(QSize(60, 24));
        ui->satsTableWidget->setItem(i, 5, newItem6);
        QTableWidgetItem* newItem7 = new QTableWidgetItem(QString::number(newlist[i].fQuality));
        color = Qt::green;
        float transp = 0.166 * (newlist[i].fQuality - 1);
        if (transp < 0.)
            transp = 0.;
        if (transp > 1.)
            transp = 1.;
        color.setAlphaF(transp);
        newItem7->setBackground(color);

        newItem7->setSizeHint(QSize(25, 24));
        ui->satsTableWidget->setItem(i, 6, newItem7);
        str = "n/a";
        if (newlist[i].fHealth < GNSS_HEALTH_STRINGS.size())
            str = GNSS_HEALTH_STRINGS[newlist[i].fHealth];
        if (newlist[i].fHealth == 0) {
            color = Qt::lightGray;
        } else if (newlist[i].fHealth == 1) {
            color = Qt::green;
        } else if (newlist[i].fHealth >= 2) {
            color = Qt::red;
        }
        QTableWidgetItem* newItem8 = new QTableWidgetItem(str);
        newItem8->setSizeHint(QSize(25, 24));
        ui->satsTableWidget->setItem(i, 7, newItem8);
        int orbSrc = newlist[i].fOrbitSource;
        if (orbSrc < 0)
            orbSrc = 0;
        if (orbSrc > 7)
            orbSrc = 7;
        QTableWidgetItem* newItem9 = new QTableWidgetItem(GNSS_ORBIT_SRC_STRING[orbSrc]);
        newItem9->setSizeHint(QSize(50, 24));
        ui->satsTableWidget->setItem(i, 8, newItem9);
        QTableWidgetItem* newItem10 = new QTableWidgetItem();
        newItem10->setCheckState(Qt::CheckState::Unchecked);
        if (newlist[i].fUsed)
            newItem10->setCheckState(Qt::CheckState::Checked);
        newItem10->setFlags(newItem10->flags() & (~Qt::ItemIsUserCheckable)); // disables checkbox edit from user
        newItem10->setFlags(newItem10->flags() & (~Qt::ItemIsEditable));
        newItem10->setSizeHint(QSize(20, 24));
        ui->satsTableWidget->setItem(i, 9, newItem10);
        QTableWidgetItem* newItem11 = new QTableWidgetItem();
        newItem11->setCheckState(Qt::CheckState::Unchecked);
        if (newlist[i].fDiffCorr)
            newItem11->setCheckState(Qt::CheckState::Checked);
        newItem11->setFlags(newItem11->flags() & (~Qt::ItemIsUserCheckable)); // disables checkbox edit from user
        newItem11->setFlags(newItem11->flags() & (~Qt::ItemIsEditable));
        newItem11->setSizeHint(QSize(20, 24));
        ui->satsTableWidget->setItem(i, 10, newItem11);
    }
}

void GpsSatsForm::onTimeAccReceived(quint32 acc)
{
    double tAcc = acc * 1e-9;
    ui->timePrecisionLabel->setText(printReadableFloat(tAcc) + "s");
}

void GpsSatsForm::onFreqAccReceived(quint32 acc)
{
    double fAcc = acc * 1e-12;
    ui->freqPrecisionLabel->setText(printReadableFloat(fAcc) + "s/s");
}

void GpsSatsForm::onIntCounterReceived(quint32 cnt)
{
    ui->intCounterLabel->setText(QString::number(cnt));
}

void GpsSatsForm::onGpsMonHWReceived(const GnssMonHwStruct& hwstruct)
{
    ui->lnaNoiseLabel->setText(QString::number(-hwstruct.noise) + " dBHz");
    ui->lnaAgcLabel->setText(QString::number(hwstruct.agc));
    QString str = "n/a";
    if (hwstruct.antStatus < GNSS_ANT_STATUS_STRINGS.size())
        str = GNSS_ANT_STATUS_STRINGS[hwstruct.antStatus];
    switch (hwstruct.antStatus) {
    case 0:
        ui->antStatusLabel->setStyleSheet("QLabel { background-color : yellow }");
        break;
    case 2:
        ui->antStatusLabel->setStyleSheet("QLabel { background-color : Window }");
        break;
    case 3:
        ui->antStatusLabel->setStyleSheet("QLabel { background-color : red }");
        break;
    case 4:
        ui->antStatusLabel->setStyleSheet("QLabel { background-color : red }");
        break;
    case 1:
    default:
        ui->antStatusLabel->setStyleSheet("QLabel { background-color : yellow }");
    }
    ui->antStatusLabel->setText(str);
    switch (hwstruct.antPower) {
    case 0:
        str = "off";
        break;
    case 1:
        str = "on";
        break;
    case 2:
    default:
        str = "unknown";
    }
    ui->antPowerLabel->setText(str);
    ui->jammingProgressBar->setValue(hwstruct.jamInd / 2.55);
}

void GpsSatsForm::onGpsMonHW2Received(const GnssMonHw2Struct& hw2struct)
{
    const int iqPixmapSize = 65;
    QPixmap iqPixmap(iqPixmapSize, iqPixmapSize);
    iqPixmap.fill(Qt::white);
    QPainter iqPainter(&iqPixmap);
    iqPainter.setPen(QPen(Qt::black));
    iqPainter.drawLine(QPoint(iqPixmapSize / 2, 0), QPoint(iqPixmapSize / 2, iqPixmapSize));
    iqPainter.drawLine(QPoint(0, iqPixmapSize / 2), QPoint(iqPixmapSize, iqPixmapSize / 2));
    QColor col(Qt::blue);
    iqPainter.setPen(col);
    double x = 0., y = 0.;
    for (int i = 0; i < iqTrack.size(); i++) {
        iqPainter.drawPoint(iqTrack[i]);
    }
    x = hw2struct.ofsI * iqPixmapSize / (2 * 127) + iqPixmapSize / 2.;
    y = -hw2struct.ofsQ * iqPixmapSize / (2 * 127) + iqPixmapSize / 2.;
    col.setAlpha(100);
    iqPainter.setBrush(col);
    iqPainter.drawEllipse(QPointF(x, y), hw2struct.magI * iqPixmapSize / 512., hw2struct.magQ * iqPixmapSize / 512.);
    ui->iqAlignmentLabel->setPixmap(iqPixmap);
    iqTrack.push_back(QPointF(x, y));
    if (iqTrack.size() > MAX_IQTRACK_BUFFER)
        iqTrack.pop_front();
}

void GpsSatsForm::onGpsVersionReceived(const QString& swString, const QString& hwString, const QString& protString)
{
    ui->ubxHwVersionLabel->setText(hwString);
    ui->ubxSwVersionLabel->setText(swString);
    ui->UBXprotLabel->setText(protString);
}

void GpsSatsForm::onGpsFixReceived(quint8 val)
{
    QString fixType = "N/A";
    if (val < FIX_TYPE_STRINGS.size())
        fixType = FIX_TYPE_STRINGS[val];
    if (val < 2)
        ui->fixTypeLabel->setStyleSheet("QLabel { background-color : red }");
    else if (val == 2)
        ui->fixTypeLabel->setStyleSheet("QLabel { background-color : lightgreen }");
    else if (val > 2)
        ui->fixTypeLabel->setStyleSheet("QLabel { background-color : green }");
    else
        ui->fixTypeLabel->setStyleSheet("QLabel { background-color : Window }");
    ui->fixTypeLabel->setText(fixType);
}

void GpsSatsForm::onGeodeticPosReceived(const GeodeticPos& pos)
{

    QString str;
    str = printReadableFloat(pos.hAcc / 1000., 2, 4) + "m / " + printReadableFloat(pos.vAcc / 1000., 2, 4) + "m";
    ui->xyzResLabel->setText(str);
}

void GpsSatsForm::onUiEnabledStateChange(bool connected)
{
    if (!connected) {
        QVector<GnssSatellite> emptylist;
        onSatsReceived(emptylist);
        iqTrack.clear();
        onGpsMonHW2Received(GnssMonHw2Struct());
        ui->jammingProgressBar->setValue(0);
        ui->timePrecisionLabel->setText("N/A");
        ui->freqPrecisionLabel->setText("N/A");
        ui->intCounterLabel->setText("N/A");
        ui->lnaNoiseLabel->setText("N/A");
        ui->lnaAgcLabel->setText("N/A");
        ui->antStatusLabel->setText("N/A");
        ui->antPowerLabel->setText("N/A");
        ui->fixTypeLabel->setText("N/A");
        ui->fixTypeLabel->setStyleSheet("QLabel { background-color : Window }");
        ui->xyzResLabel->setText("N/A");
        ui->ubxHwVersionLabel->setText("N/A");
        ui->ubxSwVersionLabel->setText("N/A");
        ui->UBXprotLabel->setText("N/A");
        ui->ubxUptimeLabel->setText("N/A");
    }
    ui->jammingProgressBar->setEnabled(connected);
    iqTrack.clear();
    this->setEnabled(connected);
}

void GpsSatsForm::onUbxUptimeReceived(quint32 val)
{
    ui->ubxUptimeLabel->setText(" " + QString::number(val / 3600., 'f', 2) + " h ");
}
