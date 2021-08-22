#ifndef I2CFORM_H
#define I2CFORM_H

#include <QString>
#include <QVector>
#include <QWidget>

struct I2cDeviceEntry;

namespace Ui {
class I2cForm;
}

class I2cForm : public QWidget {
    Q_OBJECT

public:
    explicit I2cForm(QWidget* parent = nullptr);
    ~I2cForm();

signals:
    void i2cStatsRequest();
    void scanI2cBusRequest();

public slots:
    void onI2cStatsReceived(quint32 bytesRead, quint32 bytesWritten, const QVector<I2cDeviceEntry>& deviceList);
    void onUiEnabledStateChange(bool connected);

private slots:
    void on_statsQueryPushButton_clicked();

    void on_scanBusPushButton_clicked();

private:
    Ui::I2cForm* ui;
};

#endif // I2CFORM_H
