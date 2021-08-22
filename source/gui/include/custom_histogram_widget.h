#ifndef CUSTOMHISTOGRAM_H
#define CUSTOMHISTOGRAM_H

#include "histogram.h"

#include <QVector>
#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_histogram.h>
#include <qwt_series_data.h>

class QwtPlotHistogram;
class Histogram;

class CustomHistogram : public QwtPlot, public Histogram {
    Q_OBJECT
signals:
    void histogramCleared(QString histogramName);

public:
    CustomHistogram(QWidget* parent = 0)
        : QwtPlot(parent)
    {
        initialize();
    }
    CustomHistogram(const QwtText& title_l, QWidget* parent = 0)
        : QwtPlot(title_l, parent)
    {
        initialize();
    }
    ~CustomHistogram();
    void initialize();
    bool enabled() const { return fEnabled; }
    QwtPlotGrid* grid = nullptr;
    QString title = "Histogram";

public slots:
    void update();
    void clear();
    void setEnabled(bool status);
    void setXMin(double val);
    void setXMax(double val);
    bool getLogX() const { return fLogX; }
    void setLogY(bool);
    bool getLogY() const { return fLogY; }
    void rescalePlot();

    QwtPlotHistogram* getHistogramPlot() { return fBarChart; }

    void setData(const Histogram& hist);

private slots:
    void popUpMenu(const QPoint& pos);
    void exportToFile();

private:
    QwtPlotHistogram* fBarChart = nullptr;
    bool fLogY = false;
    bool fLogX = false;
    bool fEnabled { false };
};

#endif // CUSTOMHISTOGRAM_H
