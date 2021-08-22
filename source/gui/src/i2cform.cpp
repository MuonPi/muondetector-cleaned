#include <i2cform.h>
#include <muondetector_structs.h>
#include <ui_i2cform.h>

I2cForm::I2cForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::I2cForm)
{
    ui->setupUi(this);
}

I2cForm::~I2cForm()
{
    delete ui;
}

void I2cForm::onI2cStatsReceived(quint32 bytesRead, quint32 bytesWritten, const QVector<I2cDeviceEntry>& deviceList)
{
    ui->nrDevicesLabel->setText("Nr. of devices: " + QString::number(deviceList.size()));
    ui->bytesReadLabel->setText("total bytes read: " + QString::number(bytesRead));
    ui->bytesWrittenLabel->setText("total bytes written: " + QString::number(bytesWritten));

    ui->devicesTableWidget->setRowCount(deviceList.size());
    for (int i = 0; i < deviceList.size(); i++) {
        QTableWidgetItem* newItem1 = new QTableWidgetItem("0x" + QString("%1").arg(deviceList[i].address, 2, 16, QChar('0')));
        newItem1->setSizeHint(QSize(120, 24));
        newItem1->setTextAlignment(Qt::AlignCenter);
        ui->devicesTableWidget->setItem(i, 0, newItem1);

        QTableWidgetItem* newItem2 = new QTableWidgetItem(deviceList[i].name);
        newItem2->setSizeHint(QSize(200, 24));
        newItem2->setTextAlignment(Qt::AlignCenter);
        ui->devicesTableWidget->setItem(i, 1, newItem2);

        uint8_t status = deviceList[i].status;
        QString str;
        QBrush brush = QBrush(Qt::green, Qt::SolidPattern);
        if (status == 0) {
            str = "unknown";
            brush = QBrush(Qt::lightGray, Qt::SolidPattern);
        }
        if (status & 0x04) {
            str = "missing";
            brush = QBrush(Qt::red, Qt::SolidPattern);
        } else {
            if (status & 0x01) {
                str = "online";
                brush = QBrush(Qt::green, Qt::SolidPattern);
            }
            if (status & 0x02) {
                str = "system";
                brush = QBrush(Qt::blue, Qt::SolidPattern);
            }
        }
        if (status & 0x08) {
            str = "bus error";
            brush = QBrush(Qt::red, Qt::SolidPattern);
        }
        if (status & 0x10) {
            str = "locked";
            brush = QBrush(Qt::darkGray, Qt::SolidPattern);
        }

        QTableWidgetItem* newItem3 = new QTableWidgetItem(str);
        newItem3->setBackground(brush);
        newItem3->setSizeHint(QSize(140, 24));
        newItem3->setTextAlignment(Qt::AlignCenter);
        ui->devicesTableWidget->setItem(i, 2, newItem3);
    }
}

void I2cForm::onUiEnabledStateChange(bool connected)
{
    if (!connected) {
        ui->nrDevicesLabel->setText("Nr. of devices: ");
        ui->bytesReadLabel->setText("total bytes read: ");
        ui->bytesWrittenLabel->setText("total bytes written: ");
        ui->devicesTableWidget->setRowCount(0);
    }
    this->setEnabled(connected);
}

void I2cForm::on_statsQueryPushButton_clicked()
{
    emit i2cStatsRequest();
}

void I2cForm::on_scanBusPushButton_clicked()
{
    emit scanI2cBusRequest();
}
