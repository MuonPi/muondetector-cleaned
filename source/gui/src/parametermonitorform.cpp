#include <qwt_symbol.h>
#include "parametermonitorform.h"
#include "ui_parametermonitorform.h"
#include <muondetector_structs.h>

ParameterMonitorForm::ParameterMonitorForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ParameterMonitorForm)
{
    ui->setupUi(this);
    ui->adcTracePlot->setTitle("ADC trace");
    ui->adcTracePlot->setAxisTitle(QwtPlot::xBottom,"sample nr. since trigger");
    ui->adcTracePlot->setAxisTitle(QwtPlot::yLeft,"U / V");

    ui->adcTracePlot->addCurve("curve1", Qt::blue);
    ui->adcTracePlot->curve("curve1").setStyle(QwtPlotCurve::NoCurve);
    QwtSymbol *sym=new QwtSymbol(QwtSymbol::Rect, QBrush(Qt::blue, Qt::SolidPattern),QPen(Qt::black),QSize(5,5));
    ui->adcTracePlot->curve("curve1").setSymbol(sym);
    ui->adcTracePlot->setAxisAutoScale(QwtPlot::xBottom, false);
    ui->adcTracePlot->setAxisAutoScale(QwtPlot::yLeft, false);
    ui->adcTracePlot->setAxisScale(QwtPlot::xBottom, -10., 40. );
    ui->adcTracePlot->setAxisScale(QwtPlot::yLeft, 0., 3.5 );
    ui->adcTracePlot->replot();
    foreach (GpioSignalDescriptor item, GPIO_SIGNAL_MAP) {
        if (item.direction==DIR_IN) ui->adcTriggerSelectionComboBox->addItem(item.name);
    }
    ui->timingSelectionComboBox->clear();
    foreach (QString item, TIMING_MUX_SIGNAL_NAMES) {
        ui->timingSelectionComboBox->addItem(item);
    }
    connect(ui->adcTraceGroupBox, &QGroupBox::clicked, this, [this](bool checked)
        {
            emit adcModeChanged((checked)?ADC_MODE_TRACE:ADC_MODE_PEAK);
        } );
    connect( ui->preamp1EnCheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::preamp1EnableChanged );
    connect( ui->preamp2EnCheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::preamp2EnableChanged );
    connect( ui->biasEnCheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::biasEnableChanged );
    connect( ui->hiGainCheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::gainSwitchChanged );
	connect( ui->pol1CheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::onPolarityCheckBoxClicked );
    connect( ui->pol2CheckBox, &QCheckBox::clicked, this, &ParameterMonitorForm::onPolarityCheckBoxClicked );

    ui->adcTracePlot->setMinimumHeight(30);
}

void ParameterMonitorForm::on_timingSelectionComboBox_currentIndexChanged(int index)
{
    emit timingSelectionChanged(index);
}

void ParameterMonitorForm::on_adcTriggerSelectionComboBox_currentIndexChanged(int index)
{
    for (auto signalIt=GPIO_SIGNAL_MAP.begin(); signalIt!=GPIO_SIGNAL_MAP.end(); ++signalIt) {
        const GPIO_SIGNAL signalId=signalIt.key();
        if (GPIO_SIGNAL_MAP[signalId].name==ui->adcTriggerSelectionComboBox->itemText(index)) {
            emit triggerSelectionChanged(signalId);
            return;
        }
    }
}

ParameterMonitorForm::~ParameterMonitorForm()
{
    delete ui;
}

void ParameterMonitorForm::onCalibReceived(bool /*valid*/, bool /*eepromValid*/, quint64 /*id*/, const QVector<CalibStruct> & calibList)
{
    fCalibList.clear();
    for (int i=0; i<calibList.size(); i++)
    {
        fCalibList.push_back(calibList[i]);
    }

    int ver = getCalibParameter("VERSION").toInt();
    ui->hwVersionLabel->setText(QString::number(ver));
/*
    double rsense = 0.1*getCalibParameter("RSENSE").toInt();
    ui->rsenseDoubleSpinBox->setValue(rsense);
    double vdiv = 0.01*getCalibParameter("VDIV").toInt();
    ui->vdivDoubleSpinBox->setValue(vdiv);
    int eepCycles = getCalibParameter("WRITE_CYCLES").toInt();
    ui->eepromWriteCyclesLabel->setText(QString::number(eepCycles));
    int featureFlags = getCalibParameter("FEATURE_FLAGS").toInt();
    ui->featureGnssCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_GNSS);
    ui->featureEnergyCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_ENERGY);
    ui->featureDetBiasCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_DETBIAS);
    ui->featurePreampBiasCheckBox->setChecked(featureFlags & CalibStruct::FEATUREFLAGS_PREAMP_BIAS);
*/
/*
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
*/
}


void ParameterMonitorForm::onAdcSampleReceived(uint8_t channel, float value)
{
    if (channel==0)
        ui->adcLabel1->setText(QString::number(value,'f',4));
    else if (channel==1)
        ui->adcLabel2->setText(QString::number(value,'f',4));
    else if (channel==2) {
        ui->adcLabel3->setText(QString::number(value,'f',4));
        fLastBiasVoltageHi = value;
    }
    else if (channel==3) {
        ui->adcLabel4->setText(QString::number(value,'f',4));

        if (!fCalibList.empty()) {
            double vdiv=getCalibParameter("VDIV").toDouble()*0.01;
            double ubias = value*vdiv;
            ui->biasVoltageLabel->setText(QString::number(ubias,'f',2));
            emit biasVoltageCalculated(ubias);

            double ioffs = 0.;
            if (currentCalibValid()) {
                double fSlope2 = getCalibParameter("COEFF3").toDouble();
                double fOffs2 = getCalibParameter("COEFF2").toDouble();
                ioffs = ubias*fSlope2+fOffs2;

            }
            double rsense = getCalibParameter("RSENSE").toDouble()*0.1/1000.; // RSense in MOhm
            double ibias = (fLastBiasVoltageHi-value)*vdiv/rsense-ioffs;

            if (fLastBiasVoltageHi>-900.) {
                emit biasCurrentCalculated(ibias);
                ui->biasCurrentLabel->setText(QString::number(ibias,'f',1));
            }
        }
        fLastBiasVoltageLo = value;
    }
}

void ParameterMonitorForm::onDacReadbackReceived(uint8_t channel, float value)
{
    ui->dacSpinBox1->blockSignals(true);
    ui->dacSpinBox2->blockSignals(true);
    ui->dacSpinBox3->blockSignals(true);
    ui->dacSpinBox4->blockSignals(true);
    ui->dacSlider1->blockSignals(true);
    ui->dacSlider2->blockSignals(true);
    ui->dacSlider3->blockSignals(true);
    ui->dacSlider4->blockSignals(true);
    if (channel==0) {
        ui->dacSpinBox1->setValue(value);
        ui->dacSlider1->setValue(value*1000);
    } else if (channel==1) {
        ui->dacSpinBox2->setValue(value);
        ui->dacSlider2->setValue(value*1000);
    } else if (channel==2) {
        ui->dacSpinBox3->setValue(value);
        ui->dacSlider3->setValue(value*1000);
    } else if (channel==3) {
        ui->dacSpinBox4->setValue(value);
        ui->dacSlider4->setValue(value*1000);
    }
    ui->dacSpinBox1->blockSignals(false);
    ui->dacSpinBox2->blockSignals(false);
    ui->dacSpinBox3->blockSignals(false);
    ui->dacSpinBox4->blockSignals(false);
    ui->dacSlider1->blockSignals(false);
    ui->dacSlider2->blockSignals(false);
    ui->dacSlider3->blockSignals(false);
    ui->dacSlider4->blockSignals(false);
}

void ParameterMonitorForm::onInputSwitchReceived(uint8_t index)
{
    ui->timingSelectionComboBox->setCurrentIndex(index);
}

void ParameterMonitorForm::onBiasSwitchReceived(bool state)
{
    ui->biasEnCheckBox->setChecked(state);
}

void ParameterMonitorForm::onPreampSwitchReceived(uint8_t channel, bool state)
{
    if (channel==0) ui->preamp1EnCheckBox->setChecked(state);
    else if (channel==1) ui->preamp2EnCheckBox->setChecked(state);
}

void ParameterMonitorForm::onTriggerSelectionReceived(GPIO_SIGNAL signal)
{
    int i=0;
    while (i<ui->adcTriggerSelectionComboBox->count()) {
        if (ui->adcTriggerSelectionComboBox->itemText(i).compare(GPIO_SIGNAL_MAP[signal].name)==0) break;
        i++;
    }
    if (i>=ui->adcTriggerSelectionComboBox->count()) return;
    ui->adcTriggerSelectionComboBox->blockSignals(true);
    ui->adcTriggerSelectionComboBox->setEnabled(true);
    ui->adcTriggerSelectionComboBox->setCurrentIndex(i);
    ui->adcTriggerSelectionComboBox->blockSignals(false);
}

void ParameterMonitorForm::onGainSwitchReceived(bool state)
{
    ui->hiGainCheckBox->setChecked(state);
}

void ParameterMonitorForm::onTemperatureReceived(float temp)
{
    ui->temperatureLabel->setText(QString::number(temp, 'f', 2));
}

void ParameterMonitorForm::onTimepulseReceived()
{
    //
}

void ParameterMonitorForm::onTimeMarkReceived(const UbxTimeMarkStruct& tm)
{
    ui->ubloxCounterLabel->setText(QString::number(tm.evtCounter));
}

void ParameterMonitorForm::onAdcTraceReceived(const QVector<float> &sampleBuffer)
{
    QVector<QPointF> vec;
    for (int i=0; i<sampleBuffer.size(); i++) {
        QPointF p1;
        p1.rx()=i-9;
        p1.ry()=sampleBuffer[i];
        vec.push_back(p1);
    }

    ui->adcTracePlot->curve("curve1").setSamples(vec);
    ui->adcTracePlot->replot();

}

void ParameterMonitorForm::onTimeAccReceived(quint32 acc)
{
    ui->timePrecLabel->setText(QString::number(acc*1e-9, 'g', 6));
}

void ParameterMonitorForm::onFreqAccReceived(quint32 /*acc*/)
{//

}

void ParameterMonitorForm::onIntCounterReceived(quint32 /*cnt*/)
{
    //ui->ubloxCounterLabel->setText(QString::number(cnt));
}

void ParameterMonitorForm::onPolaritySwitchReceived(bool pol1, bool pol2)
{
	ui->pol1CheckBox->blockSignals(true);
	ui->pol2CheckBox->blockSignals(true);
	ui->pol1CheckBox->setChecked(pol1);
	ui->pol2CheckBox->setChecked(pol2);
	ui->pol1CheckBox->blockSignals(false);
	ui->pol2CheckBox->blockSignals(false);
}


void ParameterMonitorForm::on_dacSpinBox1_valueChanged(double arg1)
{
    emit setDacVoltage(0, arg1);
}

void ParameterMonitorForm::on_dacSpinBox2_valueChanged(double arg1)
{
    emit setDacVoltage(1, arg1);
}

void ParameterMonitorForm::on_dacSpinBox3_valueChanged(double arg1)
{
    emit setDacVoltage(2, arg1);
}

void ParameterMonitorForm::on_dacSpinBox4_valueChanged(double arg1)
{
    emit setDacVoltage(3, arg1);
}

void ParameterMonitorForm::on_dacSlider1_valueChanged(int value)
{
    double voltage = value/1000.;
    emit setDacVoltage(0, voltage);
}

void ParameterMonitorForm::on_dacSlider2_valueChanged(int value)
{
    double voltage = value/1000.;
    emit setDacVoltage(1, voltage);
}

void ParameterMonitorForm::on_dacSlider3_valueChanged(int value)
{
    double voltage = value/1000.;
    emit setDacVoltage(2, voltage);
}

void ParameterMonitorForm::on_dacSlider4_valueChanged(int value)
{
    double voltage = value/1000.;
    emit setDacVoltage(3, voltage);
}

QString ParameterMonitorForm::getCalibParameter(const QString &name)
{
    if (!fCalibList.empty()) {
        auto result = std::find_if(fCalibList.begin(), fCalibList.end(), [&name](const CalibStruct& s){ return s.name==name.toStdString(); } );
        if (result != fCalibList.end()) {
            return QString::fromStdString(result->value);
        }
    }
    return "";
}

bool ParameterMonitorForm::currentCalibValid()
{
    int calibFlags = getCalibParameter("CALIB_FLAGS").toUInt();
    if (calibFlags & CalibStruct::CALIBFLAGS_CURRENT_COEFFS) return true;
    return false;
}

void ParameterMonitorForm::on_gpioInhibitCheckBox_clicked(bool checked)
{
    emit gpioInhibitChanged(checked);
//	qDebug()<<"set inhibit to "<<QString((checked)?"true":"false");
}

void ParameterMonitorForm::onPolarityCheckBoxClicked(bool /*checked*/){
//
    bool pol1=ui->pol1CheckBox->isChecked();
    bool pol2=ui->pol2CheckBox->isChecked();
    emit polarityChanged(pol1, pol2);
}

void ParameterMonitorForm::onUiEnabledStateChange(bool connected)
{
	this->setEnabled(connected);
}
