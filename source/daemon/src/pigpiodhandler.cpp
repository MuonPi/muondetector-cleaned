#include <pigpiodhandler.h>
#include <QDebug>
#include <gpio_mapping.h>
#include <exception>
#include <iostream>
#include <QPointer>
#include <time.h>
#include <sys/time.h>
#include <cmath>
#include <config.h>
#include <stdexcept>


//static int pi = -1;
static int spiHandle = -1;
//static QPointer<PigpiodHandler> pigHandlerAddress; // QPointer automatically clears itself if pigHandler object is destroyed

constexpr char CONSUMER[] = "muonpi";
const std::string chipname { "/dev/gpiochip0" };

template <typename T>
inline static T sqr(T x) { return x*x; }
//inline static long double sqr(long double x) { return x*x; }

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
void calcLinearCoefficients(int n, quint64 *xarray, qint64 *yarray,
                double *offs, double* slope,  int arrayIndex = 0)
{
   if (n<3) return;

   long double   sumx = 0.0L;                        /* sum of x                      */
   long double   sumx2 = 0.0L;                       /* sum of x**2                   */
   long double   sumxy = 0.0L;                       /* sum of x * y                  */
   long double   sumy = 0.0L;                        /* sum of y                      */
   long double   sumy2 = 0.0L;                       /* sum of y**2                   */

   int ix=arrayIndex;
   if (ix==0) ix=n-1;
   else ix--;

   quint64 offsx=xarray[ix];
   qint64 offsy=yarray[ix];
//    long long int offsy=0;

   int i;
   for (i=0; i<n; i++) {
          sumx  += xarray[i]-offsx;
          sumx2 += sqr(xarray[i]-offsx);
          sumxy += (xarray[i]-offsx) * (yarray[i]-offsy);
          sumy  += (yarray[i]-offsy);
          sumy2 += sqr(yarray[i]-offsy);
   }


   double denom = (n * sumx2 - sqr(sumx));
   if (denom == 0) {
       // singular matrix. can't solve the problem.
       *slope = 0.;
       *offs = 0.;
//       if (r) *r = 0;
       return;
   }

   long double m = (n * sumxy  -  sumx * sumy) / denom;
   long double b = (sumy * sumx2  -  sumx * sumxy) / denom;

   *slope=(double)m;
   *offs=(double)(b+offsy);
//    *offs=b;
//   printf("offsI=%lld  offsF=%f\n", offsy, b);

}


/* This is the central interrupt routine for all registered GPIO pins
 *
 */
/*
static void cbFunction(int user_pi, unsigned int user_gpio,
    unsigned int level, uint32_t tick) {
    //qDebug() << "callback user_pi: " << user_pi << " user_gpio: " << user_gpio << " level: "<< level << " pigHandlerAddressNull: " << pigHandlerAddress.isNull() ;
    if (pigHandlerAddress.isNull()) {
        //pigpio_stop(pi);
        return;
    }
    if (pi != user_pi) {
        // put some error here for the case pi is not the same as before initialized
        return;
    }

    QPointer<PigpiodHandler> pigpioHandler = pigHandlerAddress;

    if (pigpioHandler->isInhibited()) return;

    static uint32_t lastTriggerTick=0;
    static uint32_t lastXorAndTick=0;
    static uint32_t lastTick=0;
    static uint16_t pileupCounter=0;

    // look, if the last event occured just recently (less than 100us ago)
    // if so, count the pileup counter up
    // count down if not
    if (tick-lastTick<100) pileupCounter++;
    else if (pileupCounter>0) pileupCounter--;

    // if more than 50 pileups happened in a short period of time, leave immediately
    if (pileupCounter>50) return;

    try{
        // allow only registered signals to be processed here
        // if gpio pin fired which is not in GPIO_SIGNAL list, return immediately
        auto it=std::find_if(GPIO_PINMAP.cbegin(), GPIO_PINMAP.cend(),
        [&user_gpio](const std::pair<GPIO_SIGNAL, unsigned int>& val) {
            if (val.second==user_gpio) return true;
            return false;
        });
        if (it==GPIO_PINMAP.end()) return;

// 		if (user_gpio == GPIO_PINMAP[ADC_READY]) {
// 			//std::cout<<"ADC conv ready"<<std::endl;
// 			return;
//         }

		QDateTime now = QDateTime::currentDateTimeUtc();
//		qDebug()<<"gpio evt: gpio="<<user_gpio<<"  GPIO_PINMAP[EVT_XOR]="<<GPIO_PINMAP[EVT_XOR];
//		if (user_gpio == GPIO_PINMAP[EVT_AND] || user_gpio == GPIO_PINMAP[EVT_XOR]){

        if (user_gpio == GPIO_PINMAP[pigpioHandler->samplingTriggerSignal]){
            if (pigpioHandler->lastSamplingTime.msecsTo(now) >= MuonPi::Config::Hardware::ADC::deadtime) {
                emit pigpioHandler->samplingTrigger();
                pigpioHandler->lastSamplingTime = now;
            }
            quint64 nsecsElapsed=pigpioHandler->elapsedEventTimer.nsecsElapsed();
            pigpioHandler->elapsedEventTimer.start();
            //emit pigpioHandler->eventInterval(nsecsElapsed);
            emit pigpioHandler->eventInterval((tick-lastTriggerTick)*1000);
            lastTriggerTick=tick;
        }

        if (user_gpio == GPIO_PINMAP[TIMEPULSE]) {
//			std::cout<<"Timepulse"<<std::endl;
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            quint64 timestamp = pigpioHandler->gpioTickOverflowCounter + tick;
            quint64 t0 = pigpioHandler->startOfProgram.toMSecsSinceEpoch();

            long double meanDiff=pigpioHandler->clockMeasurementOffset;

            long double dx=timestamp-pigpioHandler->lastTimeMeasurementTick;
            long double dy=pigpioHandler->clockMeasurementSlope*dx;
            meanDiff+=dy;

            qint64 meanDiffInt=(qint64)meanDiff;
            double meanDiffFrac=(meanDiff-(qint64)meanDiff);
            timestamp+=meanDiffInt; // add diff to real time
            long int ts_sec=timestamp/1000000+(t0/1000);             // conv. us to s
            long int ts_nsec=1000*(timestamp%1000000)+(t0 % 1000) * 1000000L;
            ts_nsec+=(long int)(1000.*meanDiffFrac);

            long double ppsOffs = (ts_sec-ts.tv_sec)+ts_nsec*1e-9;
//			qDebug() << "PPS Offset: " << (double)(ppsOffs)*1e6 << " us";
            if (std::fabs(ppsOffs) < 3600.) {
                qint32 t_diff_us = (double)(ppsOffs)*1e6;
                emit pigpioHandler->timePulseDiff(t_diff_us);
            }
//             qint32 t_diff_us=ts.tv_nsec/1000;
//             if (t_diff_us>500000L) t_diff_us=t_diff_us-1000000L;
        }
        if (tick-lastXorAndTick > MuonPi::Config::event_count_deadtime_ticks) {
            lastXorAndTick = tick;
            emit pigpioHandler->signal(user_gpio);
        }
        // level gives the information if it is up or down (only important if trigger is
        // at both: rising and falling edge)
    }
    catch (std::exception& e)
    {
        pigpioHandler = 0;
        //pigpio_stop(pi);
        qCritical() << "Exception catched in 'static void cbFunction(int user_pi, unsigned int user_gpio, unsigned int level, uint32_t tick)':" << e.what();
        qCritical() << "with user_pi="<<user_pi<<"user_gpio="<<user_gpio<<"level="<<level<<"tick="<<tick;
    }
}
*/

PigpiodHandler::PigpiodHandler(std::vector<unsigned int> gpioPins, QObject *parent)
    : QObject(parent)
{
	startOfProgram = QDateTime::currentDateTimeUtc();
	lastSamplingTime = startOfProgram;
	elapsedEventTimer.start();

	fChip = gpiod_chip_open(chipname.c_str());
	if ( fChip == nullptr ) {
		qCritical()<<"error opening gpio chip "<<QString::fromStdString(chipname);
		throw std::exception();
		return;
	}
	isInitialised = true;

	for (unsigned int gpioPin : gpioPins) { 
		gpiod_line* line = gpiod_chip_get_line(fChip, gpioPin);
		int flags = 0;
		/*
		GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN |
		GPIOD_LINE_REQUEST_FLAG_OPEN_SOURCE |
		GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW;
		*/
		int ret = gpiod_line_request_rising_edge_events_flags( line, CONSUMER, flags);
		if ( ret < 0 ) {
			qCritical()<<"Request for registering gpio line"<<gpioPin<<"for event notification failed";
			//throw std::exception();
			continue;
			//return;
		}
		fInterruptLineMap.emplace( std::make_pair( gpioPin, line) );
	}

	gpioClockTimeMeasurementTimer.setInterval(MuonPi::Config::Hardware::GPIO::Clock::Measurement::interval/*GPIO_CLOCK_MEASUREMENT_INTERVAL_MS*/);
	gpioClockTimeMeasurementTimer.setSingleShot(false);
	connect(&gpioClockTimeMeasurementTimer, &QTimer::timeout, this, &PigpiodHandler::measureGpioClockTime);
	gpioClockTimeMeasurementTimer.start();

	reloadInterruptSettings();
}

PigpiodHandler::~PigpiodHandler() {
	this->stop();
	for ( auto [gpio,line] : fInterruptLineMap ) {
		gpiod_line_release(line);
	}
	for ( auto [gpio,line] : fLineMap ) {
		gpiod_line_release(line);
	}
	if ( fChip != nullptr )	gpiod_chip_close( fChip );
}


void PigpiodHandler::threadLoop() {
	while (fThreadRunning) {
		if ( inhibit ) { 
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			continue;
		}
		const struct timespec timeout { 0, 100000000UL };
		gpiod_line_bulk event_bulk { };
		int ret = gpiod_line_event_wait_bulk(&fInterruptLineBulk, &timeout, &event_bulk);
		if ( ret > 0 ) {
			unsigned int line_index = 0;
			while ( line_index < event_bulk.num_lines ) {
				gpiod_line_event line_event { };
				int result = gpiod_line_event_read( event_bulk.lines[line_index], &line_event);
				if ( result == 0 ) {
					unsigned int gpio = gpiod_line_offset( event_bulk.lines[line_index] );
					//std::uint64_t ns = line_event.ts.tv_sec*1e9 + line_event.ts.tv_nsec;
					//const std::chrono::nanoseconds since_epoch_ns(ns);
					std::chrono::nanoseconds since_epoch_ns(line_event.ts.tv_nsec);
					since_epoch_ns += std::chrono::seconds(line_event.ts.tv_sec);
					//auto since_epoch = std::chrono::duration_cast<std::chrono::system_clock::duration>(since_epoch_ns);
					EventTime timestamp(since_epoch_ns);
					emit event(gpio, timestamp);
					if ( verbose > 2 ) {
						qDebug()<<"line event: gpio"<<gpio<<" edge: "
						<<QString((line_event.event_type==GPIOD_LINE_EVENT_RISING_EDGE)?"rising":"falling")
						<<" ts="<<since_epoch_ns.count();
					}
				}
				line_index++;
			}
		} else if ( ret == 0 ) {
			// a timeout occurred, no event was detected
			// simply go over into the wait loop again
		} else {
			// an error occured
			// what should we do here?
			qCritical()<<"Wait for gpio line events failed";
		}
	}
}

bool PigpiodHandler::setPinInput(unsigned int gpio) {
	if (!isInitialised) return false;
	auto it = fLineMap.find(gpio);

	if (it != fLineMap.end()) {
		// line object exists, request for input
		int ret = gpiod_line_request_input( it->second, CONSUMER );
		if ( ret < 0 ) {
			qCritical()<<"Request gpio line" << gpio << "as input failed";
			return false;
		}
		return true;
	}
	// line was not allocated yet, so do it now
	gpiod_line* line = gpiod_chip_get_line(fChip, gpio);
	if ( line == nullptr ) {
		qCritical()<<"error allocating gpio line"<<gpio;
		return false;
	}
	int ret = gpiod_line_request_input( line, CONSUMER );
	if ( ret < 0 ) {
		qCritical()<<"Request gpio line" << gpio << "as input failed";
		return false;
	}
	fLineMap.emplace( std::make_pair( gpio, line ) );
	return true;
}

bool PigpiodHandler::setPinOutput(unsigned int gpio, bool initState) {
	if (!isInitialised) return false;
	auto it = fLineMap.find(gpio);

	if (it != fLineMap.end()) {
		// line object exists, request for output
		int ret = gpiod_line_request_output( it->second, CONSUMER, static_cast<int>(initState) );
		if ( ret < 0 ) {
			qCritical()<<"Request gpio line" << gpio << "as output failed";
			return false;
		}
		return true;
	}
	// line was not allocated yet, so do it now
	gpiod_line* line = gpiod_chip_get_line(fChip, gpio);
	if ( line == nullptr ) {
		qCritical()<<"error allocating gpio line"<<gpio;
		return false;
	}
	int ret = gpiod_line_request_output( line, CONSUMER, static_cast<int>(initState) );
	if ( ret < 0 ) {
		qCritical()<<"Request gpio line" << gpio << "as output failed";
		return false;
	}
	fLineMap.emplace( std::make_pair( gpio, line ) );
	return true;
}

bool PigpiodHandler::setPinBias(unsigned int gpio, std::uint8_t pin_bias) {
	if (!isInitialised) return false;
	int flags = 0;
	if ( pin_bias & PinBias::OpenDrain ) {
		flags |= GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN;
	} else if ( pin_bias & PinBias::OpenSource ) {
		flags |= GPIOD_LINE_REQUEST_FLAG_OPEN_SOURCE;
	} else if ( pin_bias & PinBias::ActiveLow ) {
		flags |= GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW;
	} else if ( pin_bias & PinBias::PullDown ) {
		//flags |= GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
	} else if ( pin_bias & PinBias::PullUp ) {
		//flags |= GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
	} else if ( !(pin_bias & PinBias::PullDown) && !(pin_bias & PinBias::PullUp) ) {
		//flags |= GPIOD_LINE_REQUEST_FLAG_BIAS_DISABLED;
	}

	auto it = fLineMap.find(gpio);
	int dir = -1;
	if (it != fLineMap.end()) {
		// line object exists, set config
		dir = gpiod_line_direction(it->second);
		gpiod_line_release(it->second);
	}
	// line was not allocated yet, so do it now
	gpiod_line* line = gpiod_chip_get_line(fChip, gpio);
	if ( line == nullptr ) {
		qCritical()<<"error allocating gpio line"<<gpio;
		return false;
	}
	int req_mode = GPIOD_LINE_REQUEST_DIRECTION_AS_IS;
	if (dir == GPIOD_LINE_DIRECTION_INPUT) req_mode = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
	else if (dir == GPIOD_LINE_DIRECTION_OUTPUT) req_mode = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
	gpiod_line_request_config config { CONSUMER, req_mode , flags };
	int ret = gpiod_line_request( line, &config, 0 );
	if ( ret < 0 ) {
		qCritical()<<"Request gpio line" << gpio << "for bias config failed";
		return false;
	}
	fLineMap.emplace( std::make_pair( gpio, line ) );
	return true;
}

bool PigpiodHandler::setPinState(unsigned int gpio, bool state) {
	if (!isInitialised) return false;
	auto it = fLineMap.find(gpio);
	if (it != fLineMap.end()) {
		// line object exists, request for output
		int ret = gpiod_line_set_value( it->second, static_cast<int>(state) );
		if ( ret < 0 ) {
			qCritical()<<"Setting state of gpio line" << gpio << "failed";
			return false;
		}
		return true;
	}
	// line was not allocated yet, so do it now
	return setPinOutput( gpio, state );
}

void PigpiodHandler::reloadInterruptSettings()
{
	this->stop();
	gpiod_line_bulk_init( &fInterruptLineBulk );
	// rerequest bulk events
	for (auto [gpio,line] : fInterruptLineMap) {
		gpiod_line_bulk_add( &fInterruptLineBulk, line );
	}
	this->start();
}

bool PigpiodHandler::registerInterrupt(unsigned int gpio, EventEdge edge) {
	if (!isInitialised) return false;
	auto it = fInterruptLineMap.find(gpio);
	if (it != fInterruptLineMap.end()) {
		// line object exists
		// The following code block shall release the previous line request
		// and re-request this line for events.
		// It is bypassed, since it does not work as intended.
		// The function simply does nothing in this case.
		return false;
		gpiod_line_release(it->second);
		if ( gpiod_line_update(it->second) < 0 ) {
			qCritical()<<"update of gpio line" << gpio << "after release failed";
			//return false;
		}
		// request for events
		int ret=-1;
		int errcnt = 10;
		while ( errcnt && ret < 0 ) {
			switch (edge) {
				case EventEdge::RisingEdge:
					ret = gpiod_line_request_rising_edge_events( it->second, CONSUMER );
					break;
				case EventEdge::FallingEdge:
					ret = gpiod_line_request_falling_edge_events( it->second, CONSUMER );
					break;
				case EventEdge::BothEdges:
					ret = gpiod_line_request_both_edges_events( it->second, CONSUMER );
				default:
					break;
			}
			errcnt--;
			if ( ret < 0 ) std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		}	
		if ( ret < 0 ) {
			qCritical()<<"Re-request gpio line" << gpio << "for events failed";
			return false;
		}
		return true;
	}
	// line was not allocated yet, so do it now
	gpiod_line* line = gpiod_chip_get_line(fChip, gpio);
	if ( line == nullptr ) {
		qCritical()<<"error allocating gpio line"<<gpio;
		return false;
	}
	int ret=-1;
	switch (edge) {
		case EventEdge::RisingEdge:
			ret = gpiod_line_request_rising_edge_events( line, CONSUMER );
			break;
		case EventEdge::FallingEdge:
			ret = gpiod_line_request_falling_edge_events( line, CONSUMER );
			break;
		case EventEdge::BothEdges:
			ret = gpiod_line_request_both_edges_events( line, CONSUMER );
		default:
			break;
	}
	if ( ret < 0 ) {
		qCritical()<<"Request gpio line" << gpio << "for events failed";
		return false;
	}
	fInterruptLineMap.emplace( std::make_pair( gpio, line ) );
	reloadInterruptSettings();
	return true;
}

bool PigpiodHandler::unRegisterInterrupt(unsigned int gpio) {
	if (!isInitialised) return false;
	auto it = fInterruptLineMap.find(gpio);
	if (it != fInterruptLineMap.end()) {
		gpiod_line_release(it->second);
		fInterruptLineMap.erase( it );
		reloadInterruptSettings();
		return true;
	}
	return false;
}

bool PigpiodHandler::initialised(){
    return isInitialised;
}

void PigpiodHandler::stop() {
	fThreadRunning = false;
	if ( fThread != nullptr ) {
		fThread->join();
	}
}

void PigpiodHandler::start() {
	if (fThreadRunning) return;
	fThreadRunning = true;
	fThread.reset(new std::thread( [this](){ this->threadLoop(); } ) );
}

void PigpiodHandler::measureGpioClockTime() {
    if (!isInitialised) return;
    static uint32_t oldTick = 0;
//    static uint64_t llTick = 0;
    const int N = MuonPi::Config::Hardware::GPIO::Clock::Measurement::buffer_size/*GPIO_CLOCK_MEASUREMENT_BUFFER_SIZE*/;
    static int nrSamples=0;
    static int arrayIndex=0;
    static qint64 diff_array[N];
    static quint64 tick_array[N];
    struct timespec tp, tp1, tp2;

    quint64 t0 = startOfProgram.toMSecsSinceEpoch();

    clock_gettime(CLOCK_REALTIME, &tp1);
///    uint32_t tick = get_current_tick(pi);
    uint32_t tick = 0;
    clock_gettime(CLOCK_REALTIME, &tp2);
//        clock_gettime(CLOCK_MONOTONIC, &tp);

    qint64 dt = tp2.tv_sec-tp1.tv_sec;
    dt *= 1000000000LL;
    dt += (tp2.tv_nsec-tp1.tv_nsec);
    dt /= 2000;

    tp = tp1;

    if (tick<oldTick)
    {
        gpioTickOverflowCounter = gpioTickOverflowCounter + UINT32_MAX + 1;
    }
    oldTick=tick;
    quint64 nr_usecs = ((quint64)tp.tv_sec*1000-t0)*1000;
    nr_usecs+=tp.tv_nsec/1000 + dt;
    diff_array[arrayIndex]=(qint64)(nr_usecs-gpioTickOverflowCounter)-tick;
    tick_array[arrayIndex]=(quint64)gpioTickOverflowCounter+tick;
    lastTimeMeasurementTick = (quint64)gpioTickOverflowCounter+tick;
    if (++arrayIndex>=N) {
        arrayIndex=0;
    }
    if (nrSamples<N) nrSamples++;

    double offs = 0.;
    double slope = 0.;
    calcLinearCoefficients(nrSamples, tick_array, diff_array, &offs, &slope, arrayIndex);
    clockMeasurementSlope = slope;
    clockMeasurementOffset = offs;

//	qDebug() << "gpio clock measurement: N=" << nrSamples << " offs=" << offs << " slope=" << slope;
}
