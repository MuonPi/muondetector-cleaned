#ifndef PIGPIODHANDLER_H
#define PIGPIODHANDLER_H

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>

#include <memory>
#include <thread>
#include <mutex>
#include <gpio_mapping.h>
#include <gpio_pin_definitions.h>
#include <muondetector_structs.h>

#include <gpiod.h>


#define XOR_RATE 0
#define AND_RATE 1
#define COMBINED_RATE 2

static QVector<unsigned int> DEFAULT_VECTOR;

class PigpiodHandler : public QObject
{
	Q_OBJECT
public:
	enum PinBias : std::uint8_t {
		BiasDisabled = 0x00,
		PullDown = 0x01,
		PullUp = 0x02,
		ActiveLow = 0x04,
		OpenDrain = 0x08,
		OpenSource = 0x10
	};
	enum class EventEdge {
		RisingEdge, FallingEdge, BothEdges
	};
	
	static constexpr unsigned int UNDEFINED_GPIO { 256 };
	
	explicit PigpiodHandler(std::vector<unsigned int> gpioPins, QObject *parent = nullptr);
	~PigpiodHandler();
	
	QDateTime startOfProgram, lastSamplingTime; // the exact time when the program starts (Utc)
	QElapsedTimer elapsedEventTimer;

	double clockMeasurementSlope=0.;
	double clockMeasurementOffset=0.;
	uint64_t gpioTickOverflowCounter=0;
	quint64 lastTimeMeasurementTick=0;
	
	bool isInhibited() const { return inhibit; }
	
signals:
	void event(unsigned int gpio_pin, EventTime timestamp);

public slots:
	void start();
	void stop();
    bool initialised();
    bool setPinInput(unsigned int gpio);
    bool setPinOutput(unsigned int gpio, bool initState);
	
	bool setPinBias(unsigned int gpio, std::uint8_t pin_bias);
    bool setPinState(unsigned int gpio, bool state);
    bool registerInterrupt(unsigned int gpio, EventEdge edge);
    bool unRegisterInterrupt(unsigned int gpio);
	void setInhibited(bool inh=true) { inhibit=inh; }

private:
    bool isInitialised = false;
    bool spiInitialised = false;
    bool spiInitialise(); // will be executed at first spi read/write command
    bool isSpiInitialised();
    unsigned int spiClkFreq;//3906250; // TDC7200: up to 20MHz SCLK
    // Raspi: Core clock speed of 250MHz can be devided by any even number from 2 to 65536
    // => 3.814kHz to 125MHz
    /*
     * spi_flags consists of the least significant 22 bits.

            21 20 19 18 17 16 15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0
            b  b  b  b  b  b  R  T  n  n  n  n  W  A u2 u1 u0 p2 p1 p0  m  m
            0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0
            word size bits  msb msb only-3wire 3wire aux CEx?  activ-low? spi-mode
    */
    unsigned int spiFlags;  // fixed value for now
    QTimer gpioClockTimeMeasurementTimer;

	void measureGpioClockTime();
	void reloadInterruptSettings();
	void threadLoop();
	
	bool inhibit=false;
	int verbose=0;
	gpiod_chip* fChip { nullptr };
	std::map<unsigned int, gpiod_line*> fInterruptLineMap { };
	gpiod_line_bulk fInterruptLineBulk;
	std::map<unsigned int, gpiod_line*> fLineMap { };
	bool fThreadRunning { false };
	std::unique_ptr<std::thread> fThread { nullptr };
	std::mutex fMutex;
};


#endif // PIGPIODHANDLER_H
