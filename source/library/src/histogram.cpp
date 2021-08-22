//#include <QDataStream>
#include <cmath>
#include <map>
#include <string>

#include "histogram.h"

Histogram::Histogram(const std::string& name, int nrBins, double min, double max)
    : fName(name)
    , fNrBins(nrBins)
    , fMin(min)
    , fMax(max)
{
}

Histogram::~Histogram()
{
    fHistogramMap.clear();
}

void Histogram::clear()
{
    fHistogramMap.clear();
    fUnderflow = fOverflow = 0.;
}

void Histogram::setName(const std::string& name)
{
    fName = name;
}

void Histogram::setUnit(const std::string& unit)
{
    fUnit = unit;
}

void Histogram::setNrBins(int bins)
{
    fNrBins = bins;
    clear();
}

int Histogram::getNrBins() const
{
    return fNrBins;
}

void Histogram::setMin(double val)
{
    fMin = val;
}

double Histogram::getMin() const
{
    return fMin;
}

void Histogram::setMax(double val)
{
    fMax = val;
}

double Histogram::getMax() const
{
    return fMax;
}

double Histogram::getRange() const
{
    return fMax - fMin;
}

double Histogram::getCenter() const
{
    return 0.5 * getRange() + fMin;
}

double Histogram::getBinCenter(int bin) const
{
    return bin2Value(bin);
}

int Histogram::getLowestOccupiedBin() const
{
    if (fHistogramMap.empty())
        return -1;
    auto it = fHistogramMap.begin();
    while (it != fHistogramMap.end() && it->second < 1e-3)
        ++it;
    if (it == fHistogramMap.end())
        return -1;
    return it->first;
}

int Histogram::getHighestOccupiedBin() const
{
    if (fHistogramMap.empty())
        return -1;
    auto it = --fHistogramMap.end();
    while (it != fHistogramMap.begin() && it->second < 1e-3)
        --it;
    if (it == fHistogramMap.begin())
        return 0;
    return it->first;
}

void Histogram::fill(double x, double mult)
{
    int bin = value2Bin(x);
    if (bin < 0) {
        fUnderflow += mult;
    } else if (bin >= fNrBins) {
        fOverflow += mult;
    } else
        fHistogramMap[bin] += mult;
}

void Histogram::setBinContent(int bin, double value)
{
    if (bin >= 0 && bin < fNrBins)
        fHistogramMap[bin] = value;
}

double Histogram::getBinContent(int bin) const
{
    if (bin >= 0 && bin < fNrBins) {
        try {
            auto it = fHistogramMap.find(bin);
            if (it != fHistogramMap.end())
                return fHistogramMap.at(bin);
            else
                return double();
        } catch (...) {
            return double();
        }
    } else
        return double();
}

double Histogram::getMean()
{
    double sum = 0., entries = 0.;
    for (const auto& entry : fHistogramMap) {
        entries += entry.second;
        sum += bin2Value(entry.first) * entry.second;
    }
    if (entries > 0.)
        return sum / entries;
    else
        return 0.;
}

double Histogram::getRMS()
{
    double mean = getMean();
    double sum = 0., entries = 0.;
    for (const auto& entry : fHistogramMap) {
        entries += entry.second;
        double dx = bin2Value(entry.first) - mean;
        sum += dx * dx * entry.second;
    }
    if (entries > 1.)
        return sqrt(sum / (entries - 1.));
    else
        return 0.;
}

double Histogram::getUnderflow() const
{
    return fUnderflow;
}

double Histogram::getOverflow() const
{
    return fOverflow;
}

double Histogram::getEntries()
{
    double sum = fUnderflow + fOverflow;
    for (const auto& entry : fHistogramMap) {
        sum += entry.second;
    }
    return sum;
}

int Histogram::value2Bin(double value) const
{
    double range = fMax - fMin;
    if (range <= 0.)
        return -1;
    int bin = (value - fMin) / range * (fNrBins - 1) + 0.5;
    return bin;
}

double Histogram::bin2Value(int bin) const
{
    double range = fMax - fMin;
    if (range <= 0.)
        return -1;
    double value = range * bin / (fNrBins - 1) + fMin;
    return value;
}

void Histogram::rescale(double center, double width)
{
    setMin(center - width / 2.);
    setMax(center + width / 2.);
    clear();
}

void Histogram::rescale(double center)
{
    double width = getMax() - getMin();
    rescale(center, width);
}
