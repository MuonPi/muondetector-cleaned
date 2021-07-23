#include <ratebuffer.h>
#include <iostream>

constexpr auto invalid_time = std::chrono::system_clock::time_point::min();

RateBuffer::RateBuffer(QObject *parent) : QObject(parent)
{
	
}

void RateBuffer::onEvent(unsigned int gpio, EventTime event_time ) {
//	auto now = std::chrono::steady_clock::now();
	if ( buffermap[gpio].eventbuffer.empty() ) {
		buffermap[gpio].eventbuffer.push(event_time);
		emit filteredEvent(gpio, event_time);
		return;
	}

	while ( 	!buffermap[gpio].eventbuffer.empty() 
			&& ( event_time - buffermap[gpio].eventbuffer.front() > fBufferTime) )
	{
		buffermap[gpio].eventbuffer.pop();
	}
	
	if ( !buffermap[gpio].eventbuffer.empty() ) {
		auto last_event_time = buffermap[gpio].eventbuffer.back();
		buffermap[gpio].last_interval = std::chrono::duration_cast<std::chrono::nanoseconds>( event_time - last_event_time );
		if ( event_time - last_event_time < MAX_DEADTIME ) {
//			std::cout << "now-last:"<<(now-last_event_time)/1us<<" dt="<<buffermap[gpio].current_deadtime.count()<<std::endl;
			if ( buffermap[gpio].current_deadtime < MAX_DEADTIME ) buffermap[gpio].current_deadtime++;
			if ( event_time - last_event_time < buffermap[gpio].current_deadtime ) {
				buffermap[gpio].eventbuffer.push(event_time);
				return;
			}
		} else {
			unsigned long deadtime = buffermap[gpio].current_deadtime.count();
			if ( deadtime>0 ) {
				deadtime--;
				buffermap[gpio].current_deadtime -= std::chrono::microseconds(1);
			}
		}
	}
	buffermap[gpio].eventbuffer.push(event_time);
	emit filteredEvent(gpio, event_time);
	if ( buffermap[gpio].last_interval != std::chrono::nanoseconds(0) ) {
		emit eventIntervalSignal(gpio, buffermap[gpio].last_interval);
	}
}

auto RateBuffer::avgRate(unsigned int gpio) const -> double
{
	auto it = buffermap.find(gpio);
	if (  it == buffermap.end() ) return 0.;
	if ( it->second.eventbuffer.empty() ) return 0.;
	auto end = std::chrono::system_clock::now();
	auto start = end - fBufferTime;
	if ( start > it->second.eventbuffer.front() ) start = it->second.eventbuffer.front();
	double span = 1e-6 * std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	return ( it->second.eventbuffer.size() / span );
}

auto RateBuffer::currentDeadtime(unsigned int gpio) const -> std::chrono::microseconds
{
	auto it = buffermap.find(gpio);
	if (  it == buffermap.end() ) return std::chrono::microseconds(0);
	return it->second.current_deadtime;
}

auto RateBuffer::lastInterval(unsigned int gpio) const -> std::chrono::nanoseconds
{
	auto it = buffermap.find(gpio);
	if ( it == buffermap.end() || it->second.eventbuffer.size() < 2 ) return std::chrono::nanoseconds(0);
	return it->second.last_interval;
}

auto RateBuffer::lastInterval(unsigned int gpio1, unsigned int gpio2) const -> std::chrono::nanoseconds
{
	auto it1 = buffermap.find(gpio1);
	if ( it1 == buffermap.end() || it1->second.eventbuffer.empty() ) return std::chrono::nanoseconds(0);
	auto it2 = buffermap.find(gpio2);
	if ( it2 == buffermap.end() || it2->second.eventbuffer.empty() ) return std::chrono::nanoseconds(0);
	return std::chrono::duration_cast<std::chrono::nanoseconds>( it2->second.eventbuffer.back() - it1->second.eventbuffer.back() );
}

auto RateBuffer::lastEventTime(unsigned int gpio) const -> EventTime
{
	auto it = buffermap.find(gpio);
	if ( it == buffermap.end() || it->second.eventbuffer.empty() ) return invalid_time;
	return it->second.eventbuffer.back();
}

