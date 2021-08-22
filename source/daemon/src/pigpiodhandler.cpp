#include <QDebug>
#include <QPointer>
#include <cmath>
#include <config.h>
#include <exception>
#include "utility/gpio_mapping.h"
#include <iostream>
#include <pigpiodhandler.h>
#include <sys/time.h>
#include <time.h>

extern "C" {
#include <pigpiod_if2.h>
}

static int pi = -1;
static int spiHandle = -1;
static QPointer<PigpiodHandler> pigHandlerAddress; // QPointer automatically clears itself if pigHandler object is destroyed

template <typename T>
inline static T sqr(T x) { return x * x; }

/*
 linear regression algorithm
 taken from:
   http://stackoverflow.com/questions/5083465/fast-efficient-least-squares-fit-algorithm-in-c
 parameters:
  n = number of data points
  xarray,yarray  = arrays of data
  *offs = output intercept
  *slope  = output slope
 */
void calcLinearCoefficients(int n, quint64* xarray, qint64* yarray,
    double* offs, double* slope, int arrayIndex = 0)
{
    if (n < 3)
        return;

    long double sumx = 0.0L; /* sum of x                      */
    long double sumx2 = 0.0L; /* sum of x**2                   */
    long double sumxy = 0.0L; /* sum of x * y                  */
    long double sumy = 0.0L; /* sum of y                      */
    long double sumy2 = 0.0L; /* sum of y**2                   */

    int ix = arrayIndex;
    if (ix == 0)
        ix = n - 1;
    else
        ix--;

    quint64 offsx = xarray[ix];
    qint64 offsy = yarray[ix];

    int i;
    for (i = 0; i < n; i++) {
        sumx += xarray[i] - offsx;
        sumx2 += sqr(xarray[i] - offsx);
        sumxy += (xarray[i] - offsx) * (yarray[i] - offsy);
        sumy += (yarray[i] - offsy);
        sumy2 += sqr(yarray[i] - offsy);
    }

    double denom = (n * sumx2 - sqr(sumx));
    if (denom == 0) {
        // singular matrix. can't solve the problem.
        *slope = 0.;
        *offs = 0.;
        return;
    }

    long double m = (n * sumxy - sumx * sumy) / denom;
    long double b = (sumy * sumx2 - sumx * sumxy) / denom;

    *slope = (double)m;
    *offs = (double)(b + offsy);
}

/* This is the central interrupt routine for all registered GPIO pins
 *
 */
static void cbFunction(int user_pi, unsigned int user_gpio,
    unsigned int level, uint32_t tick)
{
    if (pigHandlerAddress.isNull()) {
        pigpio_stop(pi);
        return;
    }
    if (pi != user_pi) {
        // put some error here for the case pi is not the same as before initialized
        return;
    }

    QPointer<PigpiodHandler> pigpioHandler = pigHandlerAddress;

    if (pigpioHandler->isInhibited())
        return;

    static uint32_t lastTriggerTick = 0;
    static uint32_t lastTick = 0;
    static uint16_t pileupCounter = 0;

    // look, if the last event occured just recently
    // if so, count the pileup counter up
    // count down if not
    if (tick - lastTick < MuonPi::Config::event_count_deadtime_ticks) 
    {
        pileupCounter++;
        // if more than a certain number of pileups happened in a short period of time, leave immediately
        if (pileupCounter > MuonPi::Config::event_count_max_pileups) 
        {
            pileupCounter = MuonPi::Config::event_count_max_pileups;
			lastTick = tick;
            return;
        }
    } else if (pileupCounter > 0)
    {
        pileupCounter--;
    }

    lastTick = tick;

    try {
        // allow only registered signals to be processed here
        // if gpio pin fired which is not in GPIO_PIN list, return immediately
        auto it = std::find_if(GPIO_PINMAP.cbegin(), GPIO_PINMAP.cend(),
            [&user_gpio](const std::pair<GPIO_PIN, unsigned int>& val) {
                if (val.second == user_gpio)
                    return true;
                return false;
            });
        if (it == GPIO_PINMAP.end())
            return;

        QDateTime now = QDateTime::currentDateTimeUtc();

        if (user_gpio == GPIO_PINMAP[pigpioHandler->samplingTriggerSignal]) {
            if (pigpioHandler->lastSamplingTime.msecsTo(now) >= MuonPi::Config::Hardware::ADC::deadtime) {
                emit pigpioHandler->samplingTrigger();
                pigpioHandler->lastSamplingTime = now;
            }
            quint64 nsecsElapsed = pigpioHandler->elapsedEventTimer.nsecsElapsed();
            pigpioHandler->elapsedEventTimer.start();
            emit pigpioHandler->eventInterval((tick - lastTriggerTick) * 1000);
            lastTriggerTick = tick;
        }

        if (user_gpio == GPIO_PINMAP[TIMEPULSE]) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            quint64 timestamp = pigpioHandler->gpioTickOverflowCounter + tick;
            quint64 t0 = pigpioHandler->startOfProgram.toMSecsSinceEpoch();

            long double meanDiff = pigpioHandler->clockMeasurementOffset;

            long double dx = timestamp - pigpioHandler->lastTimeMeasurementTick;
            long double dy = pigpioHandler->clockMeasurementSlope * dx;
            meanDiff += dy;

            qint64 meanDiffInt = (qint64)meanDiff;
            double meanDiffFrac = (meanDiff - (qint64)meanDiff);
            timestamp += meanDiffInt; // add diff to real time
            long int ts_sec = timestamp / 1000000 + (t0 / 1000); // conv. us to s
            long int ts_nsec = 1000 * (timestamp % 1000000) + (t0 % 1000) * 1000000L;
            ts_nsec += (long int)(1000. * meanDiffFrac);

            long double ppsOffs = (ts_sec - ts.tv_sec) + ts_nsec * 1e-9;
            if (std::fabs(ppsOffs) < 3600.) {
                qint32 t_diff_us = (double)(ppsOffs)*1e6;
                emit pigpioHandler->timePulseDiff(t_diff_us);
            }
        }
        
        emit pigpioHandler->signal(user_gpio);
         
        // level gives the information if it is up or down (only important if trigger is
        // at both: rising and falling edge)
    } catch (std::exception& e) {
        pigpioHandler = 0;
        pigpio_stop(pi);
        qCritical() << "Exception catched in 'static void cbFunction(int user_pi, unsigned int user_gpio, unsigned int level, uint32_t tick)':" << e.what();
        qCritical() << "with user_pi=" << user_pi << "user_gpio=" << user_gpio << "level=" << level << "tick=" << tick;
    }
}

PigpiodHandler::PigpiodHandler(QVector<unsigned int> gpioPins, unsigned int spi_freq, uint32_t spi_flags, QObject* parent)
    : QObject(parent)
{
    startOfProgram = QDateTime::currentDateTimeUtc();
    lastSamplingTime = startOfProgram;
    elapsedEventTimer.start();
    pigHandlerAddress = this;
    spiClkFreq = spi_freq;
    spiFlags = spi_flags;
    pi = pigpio_start((char*)"127.0.0.1", (char*)"8888");
    if (pi < 0) {
        qFatal("Could not connect to pigpio daemon. Is pigpiod running? Start with sudo pigpiod -s 1");
        return;
    }

    isInitialised = true;

    for (auto& gpioPin : gpioPins) {
        set_mode(pi, gpioPin, PI_INPUT);

        int result = callback(pi, gpioPin, RISING_EDGE, cbFunction);
        if (result < 0) {
            qCritical() << "error registering gpio callback for BCM pin" << gpioPin;
        }
    }
    gpioClockTimeMeasurementTimer.setInterval(MuonPi::Config::Hardware::GPIO::Clock::Measurement::interval);
    gpioClockTimeMeasurementTimer.setSingleShot(false);
    connect(&gpioClockTimeMeasurementTimer, &QTimer::timeout, this, &PigpiodHandler::measureGpioClockTime);
    gpioClockTimeMeasurementTimer.start();
}

void PigpiodHandler::setInput(unsigned int gpio)
{
    if (isInitialised)
        set_mode(pi, gpio, PI_INPUT);
}

void PigpiodHandler::setOutput(unsigned int gpio)
{
    if (isInitialised)
        set_mode(pi, gpio, PI_OUTPUT);
}

void PigpiodHandler::setPullUp(unsigned int gpio)
{
    if (isInitialised)
        set_pull_up_down(pi, gpio, PI_PUD_UP);
}

void PigpiodHandler::setPullDown(unsigned int gpio)
{
    if (isInitialised)
        set_pull_up_down(pi, gpio, PI_PUD_DOWN);
}

void PigpiodHandler::setGpioState(unsigned int gpio, bool state)
{
    if (isInitialised) {
        gpio_write(pi, gpio, (state) ? 1 : 0);
    }
}

void PigpiodHandler::registerForCallback(unsigned int gpio, bool edge)
{
    int result = callback(pi, gpio, edge ? FALLING_EDGE : RISING_EDGE, cbFunction);
    if (result < 0) {
        GPIO_PIN pin = bcmToGpioSignal(gpio);
        qCritical() << "error registering gpio callback for BCM pin" << GPIO_SIGNAL_MAP[pin].name;
    }
}

void PigpiodHandler::writeSpi(uint8_t command, std::string data)
{
    if (!spiInitialised) {
        if (!spiInitialise()) {
            return;
        }
    }
    char txBuf[data.size() + 1];
    txBuf[0] = (char)command;
    for (unsigned int i = 1; i < data.size() + 1; i++) {
        txBuf[i] = data[i - 1];
    }
    char rxBuf[data.size() + 1];
    if (spi_xfer(pi, spiHandle, txBuf, rxBuf, data.size() + 1) != 1 + data.size()) {
        qWarning() << "writeSpi(uint8_t, std::string): wrong number of bytes transfered";
        return;
    }
}

void PigpiodHandler::readSpi(uint8_t command, unsigned int bytesToRead)
{
    if (!spiInitialised) {
        if (!spiInitialise()) {
            return;
        }
    }

    char rxBuf[bytesToRead + 1];
    char txBuf[bytesToRead + 1];
    txBuf[0] = (char)command;
    for (unsigned int i = 1; i < bytesToRead; i++) {
        txBuf[i] = 0;
    }
    if (spi_xfer(pi, spiHandle, txBuf, rxBuf, bytesToRead + 1) != 1 + bytesToRead) {
        qWarning() << "readSpi(uint8_t, unsigned int): wrong number of bytes transfered";
        return;
    }

    std::string data;
    for (unsigned int i = 1; i < bytesToRead + 1; i++) {
        data += rxBuf[i];
    }
    emit spiData(command, data);
}

bool PigpiodHandler::initialised()
{
    return isInitialised;
}

bool PigpiodHandler::isSpiInitialised()
{
    return spiInitialised;
}

bool PigpiodHandler::spiInitialise()
{
    if (!isInitialised) {
        qCritical() << "pigpiohandler not initialised";
        return false;
    }
    if (spiInitialised) {
        return true;
    }
    spiHandle = spi_open(pi, 0, spiClkFreq, spiFlags);
    if (spiHandle < 0) {
        QString errstr = "";
        switch (spiHandle) {
        case PI_BAD_CHANNEL:
            errstr = "DMA channel not 0-15";
            break;
        case PI_BAD_SPI_SPEED:
            errstr = "bad SPI speed";
            break;
        case PI_BAD_FLAGS:
            errstr = "bad spi open flags";
            break;
        case PI_NO_AUX_SPI:
            errstr = "no auxiliary SPI on Pi A or B";
            break;
        case PI_SPI_OPEN_FAILED:
            errstr = "can't open SPI device";
            break;
        default:
            break;
        }
        qCritical() << "Could not initialise SPI bus:" << errstr;
        return false;
    }
    spiInitialised = true;
    return true;
}

void PigpiodHandler::stop()
{
    if (!isInitialised) {
        return;
    }
    isInitialised = false;
    pigpio_stop(pi);
    pigHandlerAddress.clear();
}

void PigpiodHandler::measureGpioClockTime()
{
    if (!isInitialised)
        return;
    static uint32_t oldTick = 0;
    const int N = MuonPi::Config::Hardware::GPIO::Clock::Measurement::buffer_size;
    static int nrSamples = 0;
    static int arrayIndex = 0;
    static qint64 diff_array[N];
    static quint64 tick_array[N];
    struct timespec tp, tp1, tp2;

    quint64 t0 = startOfProgram.toMSecsSinceEpoch();

    clock_gettime(CLOCK_REALTIME, &tp1);
    uint32_t tick = get_current_tick(pi);
    clock_gettime(CLOCK_REALTIME, &tp2);

    qint64 dt = tp2.tv_sec - tp1.tv_sec;
    dt *= 1000000000LL;
    dt += (tp2.tv_nsec - tp1.tv_nsec);
    dt /= 2000;

    tp = tp1;

    if (tick < oldTick) {
        gpioTickOverflowCounter = gpioTickOverflowCounter + UINT32_MAX + 1;
    }
    oldTick = tick;
    quint64 nr_usecs = ((quint64)tp.tv_sec * 1000 - t0) * 1000;
    nr_usecs += tp.tv_nsec / 1000 + dt;
    diff_array[arrayIndex] = (qint64)(nr_usecs - gpioTickOverflowCounter) - tick;
    tick_array[arrayIndex] = (quint64)gpioTickOverflowCounter + tick;
    lastTimeMeasurementTick = (quint64)gpioTickOverflowCounter + tick;
    if (++arrayIndex >= N) {
        arrayIndex = 0;
    }
    if (nrSamples < N)
        nrSamples++;

    double offs = 0.;
    double slope = 0.;
    calcLinearCoefficients(nrSamples, tick_array, diff_array, &offs, &slope, arrayIndex);
    clockMeasurementSlope = slope;
    clockMeasurementOffset = offs;
}
