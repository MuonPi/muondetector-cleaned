#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "muondetector_shared_global.h"

#include <QDataStream>
//#include <cmath>
#include <map>
#include <string>

class Histogram {
public:
    Histogram() = default;
    Histogram(const std::string& name, int nrBins, double min, double max);
    ~Histogram();
    void clear();
    void setName(const std::string& name);
    void setUnit(const std::string& unit);
    void setNrBins(int bins);
    int getNrBins() const;
    void setMin(double val);
    double getMin() const;
    void setMax(double val);
    double getMax() const;
    double getRange() const;
    double getCenter() const;
    double getBinCenter(int bin) const;
    int getLowestOccupiedBin() const;
    int getHighestOccupiedBin() const;
    void fill(double x, double mult = 1.);
    void setBinContent(int bin, double value);
    double getBinContent(int bin) const;
    double getMean();
    double getRMS();
    double getUnderflow() const;
    double getOverflow() const;
    double getEntries();
    void rescale(double center, double width);
    void rescale(double center);

    friend QDataStream& operator<<(QDataStream& out, const Histogram& h);
    friend QDataStream& operator>>(QDataStream& in, Histogram& h);

    const std::string& getName() const { return fName; }
    const std::string& getUnit() const { return fUnit; }

protected:
    int value2Bin(double value) const;
    double bin2Value(int bin) const;

    std::string fName = "defaultHisto";
    std::string fUnit = "A.U.";
    int fNrBins = 100;
    double fMin = 0.0;
    double fMax = 1.0;
    double fOverflow = 0;
    double fUnderflow = 0;
    std::map<int, double> fHistogramMap;
};

#endif // HISTOGRAM_H
