#include <QTime>
#include <plotcustom.h>
#include <qwt.h>
#include <qwt_legend.h>
#include <qwt_scale_draw.h>

class TimeScaleDraw : public QwtScaleDraw {
public:
    TimeScaleDraw(const QTime& base, const bool invert = true)
        : invertValues(invert)
        , baseTime(base)
    {
    }
    virtual QwtText label(qreal v) const
    {
        if (invertValues) {
            QTime upTime = baseTime.addSecs(-(int)v);
            return QString("- " + upTime.toString());
        } else {
            QTime upTime = baseTime.addSecs((int)v);
            return QString(upTime.toString());
        }
    }

private:
    bool invertValues;
    QTime baseTime;
};

void PlotCustom::initialize()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: white; border: 0px;");
    setAutoReplot(false);
    enableAxis(QwtPlot::yLeft, false);
    enableAxis(QwtPlot::yRight, true);
    setAxisAutoScale(QwtPlot::yRight, true);

    QwtLegend* legend = new QwtLegend(this);
    legend->setDefaultItemMode(QwtLegendData::Checkable);
    this->insertLegend(legend, QwtPlot::BottomLegend);

    const QPen grayPen(Qt::gray);
    grid.setPen(grayPen);
    grid.attach(this);

    xorCurve.setAxes(QwtPlot::xBottom, QwtPlot::yRight);
    xorCurve.setRenderHint(QwtPlotCurve::RenderAntialiased, true);
    QColor xorCurveColor = Qt::darkGreen;
    xorCurveColor.setAlphaF(0.3);
    const QPen greenPen(xorCurveColor);
    xorCurve.setTitle(QwtText("xor-curve"));
    xorCurve.setPen(greenPen);
    xorCurve.setBrush(xorCurveColor);
    xorCurve.attach(this);

    andCurve.setTitle(QwtText("and-curve"));
    andCurve.setAxes(QwtPlot::xBottom, QwtPlot::yRight);
    andCurve.setRenderHint(QwtPlotCurve::RenderAntialiased, true);
    QColor andCurveColor = Qt::darkBlue;
    andCurveColor.setAlphaF(0.3);
    const QPen bluePen(andCurveColor);
    andCurve.setPen(bluePen);
    andCurve.setBrush(andCurveColor);
    andCurve.attach(this);
    connect(legend, &QwtLegend::checked, this, [this](const QVariant& itemInfo, bool on, int /*index*/) {
        if (on) {
            if (itemInfo == "xor-curve") {
                xorCurve.show();
            }
            if (itemInfo == "xor-curve") {
                andCurve.show();
            }
        } else {
            if (itemInfo == "xor-curve") {
                xorCurve.hide();
            }
            if (itemInfo == "xor-curve") {
                andCurve.hide();
            }
        }
        this->replot();
    });
    legend->checked(QString(""), true, 0);
    legend->checked(QString(""), true, 1);
    replot();
    show();
}

void PlotCustom::plotSamples(QVector<QPointF>& samples, QwtPlotCurve& curve)
{
    if (!isEnabled())
        return;
    QVector<QPointF> someSamples;
    for (auto sample : samples) {
        someSamples.push_back(sample);
        someSamples.last().setX(sample.x() - samples.at(samples.size() - 1).x());
    }

    qreal xMin = 0.0;
    qreal xMax = 0.0;
    if (!someSamples.isEmpty()) {
        xMin = someSamples.first().x();
    }
    qreal step = (double)(int)((xMax - xMin) / 6);
    if (this->size().width() < 450) {
        step = (double)(int)((xMax - xMin) / 3);
    }
    setAxisScale(QwtPlot::xBottom, xMin, xMax, step);
    QwtPointSeriesData* dat = new QwtPointSeriesData(someSamples);
    curve.setData(dat);
    replot();
}

void PlotCustom::plotXorSamples(QVector<QPointF>& xorSamples)
{
    setPreset("");
    plotSamples(xorSamples, xorCurve);
}

void PlotCustom::plotAndSamples(QVector<QPointF>& andSamples)
{
    setPreset("");
    plotSamples(andSamples, andCurve);
}

void PlotCustom::setPreset(QString preset)
{
    if (!preset.isEmpty()) {
        xAxisPreset = preset;
    }
    if (xAxisPreset == "seconds") {
        setAxisScaleDraw(QwtPlot::xBottom, new QwtScaleDraw());
    }
    if (xAxisPreset == "hh:mm:ss") {
        setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw(QTime(0, 0, 0, 0), true));
    }
    if (xAxisPreset == "time") {
        setAxisScaleDraw(QwtPlot::xBottom, new TimeScaleDraw(QTime::currentTime(), false));
    }
    replot();
}

void PlotCustom::setStatusEnabled(bool status)
{
    if (status == true) {
        xorCurve.attach(this);
        andCurve.attach(this);
        const QPen blackPen(Qt::black);
        grid.setPen(blackPen);
        setTitle(title);
        replot();
    } else {
        xorCurve.detach();
        andCurve.detach();
        const QPen grayPen(Qt::gray);
        grid.setPen(grayPen);
        setTitle("");
        replot();
    }
}
