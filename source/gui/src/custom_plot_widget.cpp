#include <QFileDialog>
#include <QMenu>
#include <numeric>
#include <qwt.h>
#include <qwt_legend.h>
#include <qwt_plot_renderer.h>
#include <qwt_scale_engine.h>

#include <custom_plot_widget.h>

#define LOG_BASELINE 0.1

QwtPlotCurve CustomPlot::INVALID_CURVE;

CustomPlot::~CustomPlot()
{
    for (auto it = fCurveMap.begin(); it != fCurveMap.end(); ++it) {
        if (*it != nullptr)
            delete *it;
    }
    if (grid != nullptr) {
        delete grid;
        grid = nullptr;
    }
}

void CustomPlot::initialize()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: white; border: 0px;");
    setAutoReplot(false);
    enableAxis(QwtPlot::yLeft, true);
    setAxisAutoScale(QwtPlot::xBottom, true);
    setAxisAutoScale(QwtPlot::yRight, true);

    grid = new QwtPlotGrid();
    const QPen blackPen(Qt::black);
    grid->setPen(blackPen);
    grid->attach(this);

    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(popUpMenu(const QPoint&)));

    show();
}

QwtPlotCurve& CustomPlot::curve(const QString& curveName)
{
    auto it = fCurveMap.find(curveName);
    if (it != fCurveMap.end())
        return **it;
    return INVALID_CURVE;
}

void CustomPlot::addCurve(const QString& name, const QColor& curveColor)
{
    QwtPlotCurve* curve = new QwtPlotCurve();
    curve->setYAxis(QwtPlot::yLeft);
    curve->setRenderHint(QwtPlotCurve::RenderAntialiased, true);
    const QPen pen(curveColor);
    curve->setPen(pen);
    fCurveMap[name] = curve;
    fCurveMap[name]->attach(this);
}

void CustomPlot::popUpMenu(const QPoint& pos)
{
    QMenu contextMenu(tr("Context menu"), this);

    QAction action1("&Log Y", this);
    action1.setCheckable(true);
    action1.setChecked(getLogY());
    connect(&action1, &QAction::toggled, this, [this](bool checked) { this->setLogY(checked); this->replot(); });
    contextMenu.addAction(&action1);
    contextMenu.addSeparator();
    QAction action3("&Export", this);
    connect(&action3, &QAction::triggered, this, &CustomPlot::exportToFile);
    contextMenu.addAction(&action3);

    contextMenu.exec(mapToGlobal(pos));
}

void CustomPlot::rescale()
{
    double ymin = 1e30;
    double ymax = -1e30;
    double xmin = 1e30;
    double xmax = -1e30;

    for (auto it = fCurveMap.begin(); it != fCurveMap.end(); ++it) {
        if ((*it)->dataSize()) {
            if (xmin > (*it)->minXValue())
                xmin = (*it)->minXValue();
            if (xmax < (*it)->maxXValue())
                xmax = (*it)->maxXValue();
            if (ymin > (*it)->minYValue())
                ymin = (*it)->minYValue();
            if (ymax < (*it)->maxYValue())
                ymax = (*it)->maxYValue();
        }
    }

    if (fLogY) {
        if (this->axisInterval(QwtPlot::yLeft).minValue() <= 0.) {
        }
        if (this->axisAutoScale(QwtPlot::yLeft)) {
        } else {
        }
    } else {
    }
}

void CustomPlot::setLogY(bool logscale)
{
    if (logscale) {
        if (this->axisInterval(QwtPlot::yLeft).minValue() <= 0.) {
            this->setAxisScale(QwtPlot::yLeft, LOG_BASELINE, axisInterval(QwtPlot::yLeft).maxValue());
        }
#if QWT_VERSION > 0x060100
        setAxisScaleEngine(QwtPlot::yLeft, new QwtLogScaleEngine());
#else
        setAxisScaleEngine(QwtPlot::yLeft, new QwtLog10ScaleEngine());
#endif
        setAxisAutoScale(QwtPlot::yLeft, true);
        fLogY = true;
    } else {
        setAxisScaleEngine(QwtPlot::yLeft, new QwtLinearScaleEngine());
        setAxisAutoScale(QwtPlot::yLeft, true);
        fLogY = false;
    }
    emit scalingChanged();
}

void CustomPlot::setEnabled(bool enabled)
{

    if (enabled) {
        for (auto& curve : fCurveMap) {
            curve->attach(this);
        }
    } else {
        for (auto& curve : fCurveMap) {
            curve->detach();
        }
    }

    replot();
    QwtPlot::setEnabled(enabled);
}

void CustomPlot::exportToFile()
{
    QPixmap qPix = grab();
    if (qPix.isNull()) {
        qDebug("Failed to capture the plot for saving");
        return;
    }
    QString types("JPEG file (*.jpeg);;" // Set up the possible graphics formats
                  "Portable Network Graphics file (*.png);;"
                  "Bitmap file (*.bmp);;"
                  "Portable Document Format (*.pdf);;"
                  "Scalable Vector Graphics Format (*.svg);;"
                  "ASCII raw data (*.txt)");
    QString filter; // Type of filter
    QString jpegExt = ".jpeg", pngExt = ".png", tifExt = ".tif", bmpExt = ".bmp", tif2Ext = "tiff"; // Suffix for the files
    QString pdfExt = ".pdf", svgExt = ".svg";
    QString txtExt = ".txt";
    QString suggestedName = "";
    QString fn = QFileDialog::getSaveFileName(this, tr("Export Plot"),
        suggestedName, types, &filter);

    if (!fn.isEmpty()) { // If filename is not null
        if (fn.contains(jpegExt)) { // Remove file extension if already there
            fn.remove(jpegExt);
        } else if (fn.contains(pngExt)) {
            fn.remove(pngExt);
        } else if (fn.contains(bmpExt)) {
            fn.remove(bmpExt);
        } else if (fn.contains(pdfExt)) {
            fn.remove(pdfExt);
        } else if (fn.contains(svgExt)) {
            fn.remove(svgExt);
        } else if (fn.contains(txtExt)) {
            fn.remove(txtExt);
        }

        if (filter.contains(jpegExt)) { // OR, Test to see if jpeg and save
            fn += jpegExt;
            qPix.save(fn, "JPEG");
        } else if (filter.contains(pngExt)) { // OR, Test to see if png and save
            fn += pngExt;
            qPix.save(fn, "PNG");
        } else if (filter.contains(bmpExt)) { // OR, Test to see if bmp and save
            fn += bmpExt;
            qPix.save(fn, "BMP");
        } else if (filter.contains(pdfExt)) {
            fn += pdfExt;
            QwtPlotRenderer renderer(this);
            renderer.renderDocument(this, fn, "pdf", QSizeF(297 / 2, 210 / 2), 72);
        } else if (filter.contains(svgExt)) {
            fn += svgExt;
            QwtPlotRenderer renderer(this);
            renderer.renderDocument(this, fn, "svg", QSizeF(297 / 2, 210 / 2), 72);
        } else if (filter.contains(txtExt)) {
            fn += txtExt;
            // export histo in asci raw data format
            QFile file(fn);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return;
            QTextStream out(&file);

            foreach (QwtPlotCurve* curve, fCurveMap) {
                out << "# " << this->title().text() << "\n";
                out << "# " << curve->title().text() << "\n";
                out << "# " << this->axisTitle(QwtPlot::xBottom).text() << " " << this->axisTitle(QwtPlot::yLeft).text() << "\n";
                for (size_t i = 0; i < curve->dataSize(); i++) {
                    out << QString::number(curve->sample(static_cast<int>(i)).x(), 'g', 10) << "  " << QString::number(curve->sample(static_cast<int>(i)).y(), 'g', 10) << "\n";
                }
            }
        }
    }
}
