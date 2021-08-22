#ifndef CUSTOMPLOT_H
#define CUSTOMPLOT_H
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_series_data.h>

class CustomPlot : public QwtPlot {
    Q_OBJECT
public:
    CustomPlot(QWidget* parent = 0)
        : QwtPlot(parent)
    {
        initialize();
    }
    CustomPlot(const QwtText& title, QWidget* parent = 0)
        : QwtPlot(title, parent)
    {
        initialize();
    }
    ~CustomPlot();
    void initialize();
    QwtPlotGrid* grid = nullptr;

    QwtPlotCurve& curve(const QString& curveName);
    void addCurve(const QString& name, const QColor& curveColor = Qt::blue);

    void setEnabled(bool enabled);

    // for other plots: subclass "CustomPlot" and put all specific functions (like below) to the new class

    static QwtPlotCurve INVALID_CURVE;

signals:
    void scalingChanged();

public slots:
    bool getLogX() const { return fLogX; }
    bool getLogY() const { return fLogY; }
    void setLogY(bool logscale);
    void rescale();
    void exportToFile();

private slots:
    void popUpMenu(const QPoint& pos);

private:
    QMap<QString, QwtPlotCurve*> fCurveMap;
    bool fLogY = false;
    bool fLogX = false;
};

#endif // CUSTOMPLOT_H
