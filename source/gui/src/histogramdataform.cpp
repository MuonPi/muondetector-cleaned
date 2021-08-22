#include "histogramdataform.h"
#include "ui_histogramdataform.h"
#include <histogram.h>

histogramDataForm::histogramDataForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::histogramDataForm)
{
    ui->setupUi(this);
    connect(ui->histoWidget, &CustomHistogram::histogramCleared, [this](const QString histogramName) {
        emit histogramCleared(histogramName);
    });
}

histogramDataForm::~histogramDataForm()
{
    delete ui;
}

void histogramDataForm::onHistogramReceived(const Histogram& h)
{
    QString name = QString::fromStdString(h.getName());
    fHistoMap[name] = h;
    updateHistoTable();
    ui->nrHistosLabel->setText(QString::number(fHistoMap.size()));
}

void histogramDataForm::updateHistoTable()
{
    ui->tableWidget->setRowCount(fHistoMap.size());
    int i = 0;
    for (auto it = fHistoMap.begin(); it != fHistoMap.end(); it++) {
        QTableWidgetItem* newItem1 = new QTableWidgetItem(it.key());
        newItem1->setSizeHint(QSize(120, 24));
        ui->tableWidget->setItem(i, 0, newItem1);
        QTableWidgetItem* newItem2 = new QTableWidgetItem(QString::number(it.value().getEntries()));
        newItem2->setSizeHint(QSize(100, 24));
        ui->tableWidget->setItem(i, 1, newItem2);
        i++;
    }
    if (fCurrentHisto.size()) {
        for (int j = 0; i < ui->tableWidget->rowCount(); j++) {
            if (ui->tableWidget->item(j, 0)->text() == fCurrentHisto) {
                on_tableWidget_cellClicked(j, 0);
            }
        }
    }
}

void histogramDataForm::on_tableWidget_cellClicked(int row, int /*column*/)
{
    QString name = ui->tableWidget->item(row, 0)->text();
    auto it = fHistoMap.find(name);
    if (it != fHistoMap.end()) {
        fCurrentHisto = name;
        ui->histoWidget->setTitle(name);
        ui->histoWidget->setData(*it);
        ui->histoWidget->setAxisTitle(QwtPlot::xBottom, QString::fromStdString(it->getUnit()));
        ui->histoWidget->rescalePlot();
        ui->histoNameLabel->setText(QString::fromStdString(it->getName()));
        ui->nrBinsLabel->setText(QString::number(it->getNrBins()));
        ui->nrEntriesLabel->setText(QString::number(it->getEntries()));
        ui->minLabel->setText(QString::number(it->getMin()));
        ui->maxLabel->setText(QString::number(it->getMax()));
        ui->underflowLabel->setText(QString::number(it->getUnderflow()));
        ui->overflowLabel->setText(QString::number(it->getOverflow()));
        ui->meanLabel->setText(QString::number(it->getMean()) + QString::fromStdString(it->getUnit()));
        ui->rmsLabel->setText(QString::number(it->getRMS(), 'g', 4) + QString::fromStdString(it->getUnit()));
    }
}

void histogramDataForm::onUiEnabledStateChange(bool connected)
{
    if (!connected) {
        ui->tableWidget->setRowCount(0);
        ui->histoWidget->clear();
        ui->histoNameLabel->setText("N/A");
        ui->nrBinsLabel->setText("N/A");
        ui->nrEntriesLabel->setText("N/A");
        ui->minLabel->setText("N/A");
        ui->maxLabel->setText("N/A");
        ui->underflowLabel->setText("N/A");
        ui->overflowLabel->setText("N/A");
        ui->meanLabel->setText("N/A");
        ui->rmsLabel->setText("N/A");
        ui->nrHistosLabel->setText(QString::number(0));
        fHistoMap.clear();
        fCurrentHisto = "";
    }
    this->setEnabled(connected);
    ui->histoWidget->setEnabled(connected);
}
