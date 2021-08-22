#ifndef PLOTCUSTOM_H
#define PLOTCUSTOM_H
#include <QPointer>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_series_data.h>

class PlotCustom : public QwtPlot {
public:
    PlotCustom(QWidget* parent = 0)
        : QwtPlot(parent)
    {
        initialize();
    }
    PlotCustom(const QwtText& title_l, QWidget* parent = 0)
        : QwtPlot(title_l, parent)
    {
        initialize();
    }

    // for other plots: subclass "PlotCustom" and put all specific functions (like below) to the new class
    void plotXorSamples(QVector<QPointF>& xorSamples);
    void plotAndSamples(QVector<QPointF>& andSamples);

    const QString title = "Rate Statistics";
public slots:
    void setPreset(QString preset = "");
    void setStatusEnabled(bool status);

private:
    void initialize();
    QString xAxisPreset = "seconds";
    void plotSamples(QVector<QPointF>& samples, QwtPlotCurve& curve);
    QwtPlotGrid grid;
    QwtPlotCurve xorCurve;
    QwtPlotCurve andCurve;
};

#endif // PLOTCUSTOM_H
