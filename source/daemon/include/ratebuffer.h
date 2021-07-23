#ifndef RATEBUFFER_H
#define RATEBUFFER_H
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <gpio_mapping.h>
#include <chrono>
#include <map>
#include <queue>
#include <list>
#include <muondetector_structs.h>

class PigpiodHandler;

constexpr double MAX_AVG_RATE { 100. };
constexpr unsigned long MAX_BURST_MULTIPLICITY { 10 };
using namespace std::literals;
constexpr std::chrono::microseconds MAX_BUFFER_TIME { 60s };
constexpr std::chrono::microseconds MAX_DEADTIME { static_cast<unsigned long>(1e+6/MAX_AVG_RATE) };

class RateBuffer : public QObject
{
	Q_OBJECT

public:
	struct BufferItem {
		std::queue< EventTime, std::list<EventTime> > eventbuffer { };
		std::chrono::microseconds current_deadtime { 0 };
		std::chrono::nanoseconds last_interval { 0 };
	};

	RateBuffer(QObject *parent = nullptr);
    ~RateBuffer() = default;
	void setRateLimit( double max_cps );
	[[nodiscard]] auto currentRateLimit() const -> double { return fRateLimit; }
	void clear() { buffermap.clear(); }
	
	[[nodiscard]] auto avgRate(unsigned int gpio) const -> double;
	[[nodiscard]] auto currentDeadtime(unsigned int gpio) const -> std::chrono::microseconds;
	[[nodiscard]] auto lastInterval(unsigned int gpio) const -> std::chrono::nanoseconds;
	[[nodiscard]] auto lastInterval(unsigned int gpio1, unsigned int gpio2) const -> std::chrono::nanoseconds;
	[[nodiscard]] auto lastEventTime(unsigned int gpio) const -> EventTime;
	
signals:
	void filteredEvent( unsigned int gpio, EventTime event_time );
	void eventIntervalSignal( unsigned int gpio, std::chrono::nanoseconds ns); 
	
public slots:
	void onEvent(unsigned int gpio, EventTime event_time );

private:
	double fRateLimit { MAX_AVG_RATE };
	std::chrono::microseconds fBufferTime { MAX_BUFFER_TIME };
	std::map<unsigned int, BufferItem> buffermap { };
	
};

#endif // RATEBUFFER_H
