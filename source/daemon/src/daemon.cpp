#include <QtNetwork>
#include <chrono>
#include <time.h>
#include <QThread>
#include <QNetworkInterface>
#include <daemon.h>
#include <gpio_pin_definitions.h>
#include <gpio_mapping.h>
//#include <calib_struct.h>
#include <ublox_messages.h>
#include <tcpmessage_keys.h>
#include <iomanip>      // std::setfill, std::setw
#include <locale>
#include <iostream>
#include <muondetector_structs.h>
#include <config.h>
//#include <logengine.h>
#include <geohash.h>
//#include <ratebuffer.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/syscall.h>

// for i2cdetect:
extern "C" {
#include "i2c/custom_i2cdetect.h"
}

#define DAC_BIAS 2 // channel of the dac where bias voltage is set
#define DAC_TH1 0 // channel of the dac where threshold 1 is set
#define DAC_TH2 1 // channel of the dac where threshold 2 is set

#define DEGREE_CHARCODE 248

// REMEMBER: "emit" keyword is just syntactic sugar and not needed AT ALL ... learned it after 1 year *clap* *clap*

using namespace std;

static unsigned int HW_VERSION = 0; // default value is set in calibration.h

int64_t msecdiff(timespec &ts, timespec &st){
    int64_t diff;
    diff = (int64_t)ts.tv_sec - (int64_t)st.tv_sec;
    diff *= 1000;
    diff += ((int64_t)ts.tv_nsec-(int64_t)st.tv_nsec)/1000000;
    return diff;
}

static QVector<uint16_t> allMsgCfgID({
    //   UBX_CFG_ANT, UBX_CFG_CFG, UBX_CFG_DAT, UBX_CFG_DOSC,
    //   UBX_CFG_DYNSEED, UBX_CFG_ESRC, UBX_CFG_FIXSEED, UBX_CFG_GEOFENCE,
    //   UBX_CFG_GNSS, UBX_CFG_INF, UBX_CFG_ITFM, UBX_CFG_LOGFILTER,
    //   UBX_CFG_MSG, UBX_CFG_NAV5, UBX_CFG_NAVX5, UBX_CFG_NMEA,
    //   UBX_CFG_ODO, UBX_CFG_PM2, UBX_CFG_PMS, UBX_CFG_PRT, UBX_CFG_PWR,
    //   UBX_CFG_RATE, UBX_CFG_RINV, UBX_CFG_RST, UBX_CFG_RXM, UBX_CFG_SBAS,
    //   UBX_CFG_SMGR, UBX_CFG_TMODE2, UBX_CFG_TP5, UBX_CFG_TXSLOT, UBX_CFG_USB
         UBX_TIM_TM2,UBX_TIM_TP,
         UBX_NAV_CLOCK, UBX_NAV_DGPS, UBX_NAV_AOPSTATUS, UBX_NAV_DOP,
         UBX_NAV_POSECEF, UBX_NAV_POSLLH, UBX_NAV_PVT, UBX_NAV_SBAS, UBX_NAV_SOL,
         UBX_NAV_STATUS, UBX_NAV_SVINFO, UBX_NAV_TIMEGPS, UBX_NAV_TIMEUTC, UBX_NAV_VELECEF,
         UBX_NAV_VELNED, /* UBX_NAV_SAT, */
         UBX_MON_HW, UBX_MON_HW2, UBX_MON_IO, UBX_MON_MSGPP,
         UBX_MON_RXBUF, UBX_MON_RXR, UBX_MON_TXBUF
    });



// signal handling stuff: put code to execute before shutdown down there
static int setup_unix_signal_handlers()
{
    struct sigaction hup, term, inte;

    hup.sa_handler = Daemon::hupSignalHandler;
    sigemptyset(&hup.sa_mask);
    hup.sa_flags = 0;
    hup.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &hup, 0)) {
        return 1;
    }

    term.sa_handler = Daemon::termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags = 0;
    term.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &term, 0)) {
        return 2;
    }

    inte.sa_handler = Daemon::intSignalHandler;
    sigemptyset(&inte.sa_mask);
    inte.sa_flags = 0;
    inte.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &inte, 0)) {
        return 3;
    }
    return 0;
}
int Daemon::sighupFd[2];
int Daemon::sigtermFd[2];
int Daemon::sigintFd[2];

void Daemon::handleSigTerm()
{
    snTerm->setEnabled(false);
    char tmp;
    ::read(sigtermFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    if (verbose > 1) {
        cout << "\nSIGTERM received" << endl;
    }

    // save ublox config in built-in flash to provide latest orbit info of sats for next start-up
    // and effectively reduce time-to-first-fix (TTFF)
    emit UBXSaveCfg();

    emit aboutToQuit();
    exit(0);
    snTerm->setEnabled(true);
}

void Daemon::handleSigHup()
{
    snHup->setEnabled(false);
    char tmp;
    ::read(sighupFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    if (verbose > 1) {
        cout << "\nSIGHUP received" << endl;
    }

    // save ublox config in built-in flash to provide latest orbit info of sats for next start-up
    // and effectively reduce time-to-first-fix (TTFF)
    emit UBXSaveCfg();

    emit aboutToQuit();
    exit(0);
    snHup->setEnabled(true);
}

void Daemon::handleSigInt()
{
    snInt->setEnabled(false);
    char tmp;
    ::read(sigintFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    if (verbose > 1) {
        cout << "\nSIGINT received" << endl;
    }
    if (showin || showout) {
        qDebug() << allMsgCfgID.size();
        qDebug() << msgRateCfgs.size();
        for (QMap<uint16_t, int>::iterator it = msgRateCfgs.begin(); it != msgRateCfgs.end(); it++) {
            qDebug().nospace() << "0x" << hex << (uint8_t)(it.key() >> 8) << " 0x" << hex << (uint8_t)(it.key() & 0xff) << " " << dec << it.value();
        }
    }

    // save ublox config in built-in flash to provide latest orbit info of sats for next start-up
    // and effectively reduce time-to-first-fix (TTFF)
    emit UBXSaveCfg();

    emit aboutToQuit();
    exit(0);
    snInt->setEnabled(true);
}


// begin of the Daemon class
Daemon::Daemon(QString username, QString password, QString new_gpsdevname, int new_verbose, quint8 new_pcaPortMask,
    float *new_dacThresh, float new_biasVoltage, bool bias_ON, bool new_dumpRaw, int new_baudrate,
    bool new_configGnss, unsigned int new_eventTrigger, QString new_peerAddress, quint16 new_peerPort,
    QString new_daemonAddress, quint16 new_daemonPort, bool new_showout, bool new_showin, bool preamp1, bool preamp2, bool gain, QString station_ID,
    bool new_polarity1, bool new_polarity2, QObject *parent)
    : QTcpServer(parent)
{
    // first, we must set the locale to be independent of the number format of the system's locale.
    // We rely on parsing floating point numbers with a decimal point (not a komma) which might fail if not setting the classic locale
    //	std::locale::global( std::locale( std::cout.getloc(), new punct_facet<char, '.'>) ) );
    //	std::locale mylocale( std::locale( std::cout.getloc(), new punct_facet<char, '.'>) );
    std::locale::global(std::locale::classic());

    qRegisterMetaType<TcpMessage>("TcpMessage");
    qRegisterMetaType<GeodeticPos>("GeodeticPos");
    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<uint16_t>("uint16_t");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int8_t>("int8_t");
    qRegisterMetaType<bool>("bool");
    qRegisterMetaType<CalibStruct>("CalibStruct");
    qRegisterMetaType<std::vector<GnssSatellite>>("std::vector<GnssSatellite>");
    qRegisterMetaType<std::vector<GnssConfigStruct>>("std::vector<GnssConfigStruct>");
    qRegisterMetaType<std::chrono::duration<double>>("std::chrono::duration<double>");
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<LogParameter>("LogParameter");
    qRegisterMetaType<UbxTimePulseStruct>("UbxTimePulseStruct");
    qRegisterMetaType<UbxDopStruct>("UbxDopStruct");
    qRegisterMetaType<timespec>("timespec");
    qRegisterMetaType<GPIO_SIGNAL>("GPIO_SIGNAL");
    qRegisterMetaType<GnssMonHwStruct>("GnssMonHwStruct");
    qRegisterMetaType<GnssMonHw2Struct>("GnssMonHw2Struct");
    qRegisterMetaType<UbxTimeMarkStruct>("UbxTimeMarkStruct");
    qRegisterMetaType<I2cDeviceEntry>("I2cDeviceEntry");
	qRegisterMetaType<PigpiodHandler::EventEdge>("PigpiodHandler::EventEdge");
    // signal handling
    setup_unix_signal_handlers();
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd)) {
        qFatal("Couldn't create HUP socketpair");
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd)) {
        qFatal("Couldn't create TERM socketpair");
    }
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd)) {
        qFatal("Couldn't createe INT socketpair");
    }

    snHup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
    connect(snHup, SIGNAL(activated(int)), this, SLOT(handleSigHup()));
    snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
    connect(snTerm, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));
    snInt = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
    connect(snInt, SIGNAL(activated(int)), this, SLOT(handleSigInt()));

    // general
    verbose = new_verbose;
    if (verbose > 4) {
        qDebug() << "daemon running in thread " << this->thread()->objectName();
    }

    if (verbose>3){
        qDebug()<<"QT version is "<<QString::number(QT_VERSION,16);
    }
    if (verbose >3){
        this->thread()->setObjectName("muondetector-daemon");
        qInfo() << this->thread()->objectName() << " thread id (pid): " << syscall(SYS_gettid);
    }

    // try to find out on which hardware version we are running
    // for this to work, we have to initialize and read the eeprom first
    // EEPROM 24AA02 type
    eep = new EEPROM24AA02();
    calib = new ShowerDetectorCalib(eep);
    if (eep->devicePresent()) {
        calib->readFromEeprom();
        if (verbose>2) {
            qInfo()<<"EEP device is present";
//			readEeprom();
//			calib->readFromEeprom();

            // Caution, only for debugging. This code snippet writes a test sequence into the eeprom
            if (1==0) {
                uint8_t buf[256];
                for (int i=0; i<256; i++) buf[i]=i;
                if (!eep->writeBytes(0, 256, buf)) cerr<<"error: write to eeprom failed!"<<endl;
                if (verbose>2) cout<<"eep write took "<<eep->getLastTimeInterval()<<" ms"<<endl;
                readEeprom();
            }
            if (1==1) {
                calib->printCalibList();
            }
        }
        uint64_t id=calib->getSerialID();
        QString hwIdStr="0x"+QString::number(id,16);
//		qInfo()<<"eep unique ID: 0x"<<hex<<id<<dec;
        qInfo()<<"EEP unique ID:"<<hwIdStr;


        //if (calib->isValid()) {

        //}
    } else {
        qCritical()<<"eeprom device NOT present!";
    }
    CalibStruct verStruct = calib->getCalibItem("VERSION");
    unsigned int version=0;
    ShowerDetectorCalib::getValueFromString(verStruct.value,version);
    if (version>0) {
        HW_VERSION=version;
        //if (verbose)
        qInfo()<<"Found HW version"<<version<<"in eeprom";
    }

    // set up the pin definitions (hw version specific)
    GPIO_PINMAP=GPIO_PINMAP_VERSIONS[HW_VERSION];

    if (verbose>1) {
        // print out the current gpio pin mapping
        // (function, gpio-pin, direction)
        cout<<"GPIO pin mapping:"<<endl;

        for (auto signalIt=GPIO_PINMAP.begin(); signalIt!=GPIO_PINMAP.end(); signalIt++) {
            const GPIO_SIGNAL signalId=signalIt->first;
            cout<<GPIO_SIGNAL_MAP[signalId].name<<"   \t: "<<signalIt->second;
            switch (GPIO_SIGNAL_MAP[signalId].direction) {
                case DIR_IN: cout<<" (in)";
                    break;
                case DIR_OUT: cout<<" (out)";
                    break;
                case DIR_IO: cout<<" (i/o)";
                    break;
                default: cout<<" (undef)";
            }
            cout<<endl;
        }
    }

    mqttHandlerThread = new QThread();
    mqttHandlerThread->setObjectName("muondetector-daemon-mqtt");
    connect(this, &Daemon::aboutToQuit, mqttHandlerThread, &QThread::quit);
    connect(mqttHandlerThread, &QThread::finished, mqttHandlerThread, &QThread::deleteLater);

    mqttHandler = new MuonPi::MqttHandler(station_ID, verbose-1);
    mqttHandler->moveToThread(mqttHandlerThread);
    connect(mqttHandler, &MuonPi::MqttHandler::mqttConnectionStatus, this, &Daemon::sendMqttStatus);
    connect(fileHandlerThread, &QThread::finished, mqttHandler, &MuonPi::MqttHandler::deleteLater);
    connect(this, &Daemon::requestMqttConnectionStatus, mqttHandler, &MuonPi::MqttHandler::onRequestConnectionStatus);
    //connect(this, &Daemon::logParameter, mqttHandler, &MqttHandler::sendLog);
    mqttHandlerThread->start();


    // create fileHandler
    fileHandlerThread = new QThread();
    fileHandlerThread->setObjectName("muondetector-daemon-filehandler");
    connect(this, &Daemon::aboutToQuit, fileHandlerThread, &QThread::quit);
    connect(fileHandlerThread, &QThread::finished, fileHandlerThread, &QThread::deleteLater);

    fileHandler = new FileHandler(username, password);
    fileHandler->moveToThread(fileHandlerThread);
    connect(this, &Daemon::aboutToQuit, fileHandler, &FileHandler::deleteLater);
    //connect(this, &Daemon::logParameter, fileHandler, &FileHandler::onReceivedLogParameter);
    connect(fileHandlerThread, &QThread::started, fileHandler, &FileHandler::start);
    connect(fileHandlerThread, &QThread::finished, fileHandler, &FileHandler::deleteLater);
    connect(fileHandler, &FileHandler::mqttConnect, mqttHandler, &MuonPi::MqttHandler::start);
    fileHandlerThread->start();

    // connect log signals to and from log engine and filehandler
    connect(this, &Daemon::logParameter, &logEngine, &LogEngine::onLogParameterReceived);
    connect(&logEngine, &LogEngine::sendLogString, mqttHandler, &MuonPi::MqttHandler::sendLog);
    connect(&logEngine, &LogEngine::sendLogString, fileHandler, &FileHandler::writeToLogFile);
    // connect to the regular log timer signal to log several non-regularly polled parameters
    connect(&logEngine, &LogEngine::logIntervalSignal, this, &Daemon::onLogParameterPolled);
    // connect the once-log flag reset slot of log engine with the logRotate signal of filehandler
    connect(fileHandler, &FileHandler::logRotateSignal, &logEngine, &LogEngine::onOnceLogTrigger);

    // instantiate, detect and initialize all other i2c devices
    // LM75A temp sensor
    lm75 = new LM75();
    if (lm75->devicePresent()) {
        if (verbose>2) {
            qInfo()<<"LM75 device is present.";
            qDebug()<<"temperature is "<<lm75->getTemperature()<< DEGREE_CHARCODE <<"C";
            qDebug()<<"readout took"<<lm75->getLastTimeInterval()<<"ms";
        }
    } else {
        qWarning()<<"LM75 device NOT present!";
    }
    // 4ch, 16/14bit ADC ADS1115/1015
    adc = new ADS1115();
    if (adc->devicePresent()) {
        adc->setPga(ADS1115::PGA4V);
        //adc->setPga(0, ADS1115::PGA2V);
//		adc->setRate(0x06);  // ADS1115::RATE475
        adc->setRate(0x07);  // ADS1115::RATE860
        adc->setAGC(false);
        if (!adc->setDataReadyPinMode()) {
            qWarning()<<"error: failed setting data ready pin mode (setting thresh regs)";
        }

        // set up sampling timer used for continuous sampling mode
        samplingTimer.setInterval(MuonPi::Config::Hardware::trace_sampling_interval);
        samplingTimer.setSingleShot(false);
        samplingTimer.setTimerType(Qt::PreciseTimer);
        connect(&samplingTimer, &QTimer::timeout, this, &Daemon::sampleAdc0TraceEvent);

        // set up peak sampling mode
        setAdcSamplingMode(ADC_MODE_PEAK);

        if (verbose>2) {
            qInfo()<<"ADS1115 device is present.";
            bool ok=adc->setLowThreshold(0b00000000);
            ok = ok && adc->setHighThreshold(0b10000000);
            if (ok) qDebug()<<"successfully setting threshold registers";
            else qWarning()<<"error: failed setting threshold registers";
            qDebug()<<"single ended channels:";
            qDebug()<<"ch0: "<<adc->readADC(0)<<" ch1: "<<adc->readADC(1)
            <<" ch2: "<<adc->readADC(2)<<" ch3: "<<adc->readADC(3);
            adc->setDiffMode(true);
            qDebug()<<"diff channels:";
            qDebug()<<"ch0-1: "<<adc->readADC(0)<<" ch0-3: "<<adc->readADC(1)
            <<" ch1-3: "<<adc->readADC(2)<<" ch2-3: "<<adc->readADC(3);
            adc->setDiffMode(false);
            qDebug()<<"readout took "<<adc->getLastTimeInterval()<<" ms";
        }
    } else {
        adcSamplingMode = ADC_MODE_DISABLED;
        qWarning()<<"ADS1115 device NOT present!";
    }

    // 4ch DAC MCP4728
    dac = new MCP4728();
    if (dac->devicePresent()) {
        if (verbose>2) {
            qInfo()<<"MCP4728 device is present.";
            qDebug()<<"DAC registers / output voltages:";
            for (int i=0; i<4; i++) {
                MCP4728::DacChannel dacChannel;
                MCP4728::DacChannel eepromChannel;
                eepromChannel.eeprom=true;
                dac->readChannel(i, dacChannel);
                dac->readChannel(i, eepromChannel);
                qDebug()<<"  ch"<<i<<": "<<dacChannel.value<<" = "<<MCP4728::code2voltage(dacChannel)<<" V"
                "  (stored: "<<eepromChannel.value<<" = "<<MCP4728::code2voltage(eepromChannel)<<" V)";
            }
            qDebug()<<"readout took "<<dac->getLastTimeInterval()<<" ms";
        }
    } else {
        qCritical("MCP4728 device NOT present!");
        // this error is critical, since the whole concept of counting muons is based on
        // the function of the threshold discriminator
        // we should quit here returning an error code (?)
        //exit(-1);
    }
    float *tempThresh = new_dacThresh;
    for (int i=std::min(DAC_TH1, DAC_TH2); i<=std::max(DAC_TH1, DAC_TH2); i++) {
        if (tempThresh[i]<0. && dac->devicePresent()) {
            MCP4728::DacChannel dacChannel;
            MCP4728::DacChannel eepromChannel;
            eepromChannel.eeprom=true;
            dac->readChannel(i, dacChannel);
            dac->readChannel(i, eepromChannel);
            tempThresh[i]=MCP4728::code2voltage(dacChannel);
            //tempThresh[i]=code2voltage(eepromChannel);
        }
    }
    dacThresh.push_back(tempThresh[0]);
    dacThresh.push_back(tempThresh[1]);


    if (dac->devicePresent()) {
        MCP4728::DacChannel eeprom_value;
        eeprom_value.eeprom = true;
        dac->readChannel(DAC_BIAS, eeprom_value);
        if (eeprom_value.value == 0) {
            setBiasVoltage(MuonPi::Config::Hardware::DAC::Voltage::bias);
            setDacThresh(0, MuonPi::Config::Hardware::DAC::Voltage::threshold[0]);
            setDacThresh(1, MuonPi::Config::Hardware::DAC::Voltage::threshold[1]);
        }
    }

    biasVoltage = new_biasVoltage;
    if (biasVoltage<0. && dac->devicePresent()) {
        MCP4728::DacChannel dacChannel;
        MCP4728::DacChannel eepromChannel;
        eepromChannel.eeprom=true;
        dac->readChannel(DAC_BIAS, dacChannel);
        dac->readChannel(DAC_BIAS, eepromChannel);
        biasVoltage=MCP4728::code2voltage(dacChannel);
        //tempThresh[i]=code2voltage(eepromChannel);
    }

    // PCA9536 4 bit I/O I2C device used for selecting the UBX timing input
    pca = new PCA9536();
    if (pca->devicePresent()) {
        if (verbose>2) {
            qInfo()<<"PCA9536 device is present."<<endl;
            qDebug()<<" inputs: 0x"<<hex<<(int)pca->getInputState();
            qDebug()<<"readout took "<<dec<<pca->getLastTimeInterval()<<" ms";
        }
        pca->setOutputPorts(0b00000111);
        setPcaChannel(new_pcaPortMask);
    } else {
        qCritical()<<"PCA9536 device NOT present!";
    }

    if (dac->devicePresent()) {
        if (dacThresh[0] > 0) dac->setVoltage(DAC_TH1, dacThresh[0]);
        if (dacThresh[1] > 0) dac->setVoltage(DAC_TH2, dacThresh[1]);
        if (biasVoltage > 0) dac->setVoltage(DAC_BIAS, biasVoltage);
    }


// removed initialization of ublox i2c interface since it doesn't work properly on RPi
// the Ublox i2c relies on clock stretching, which RPi is not supporting
// the ublox's i2c address is still occupied but locked, i.e. access is prohibited
    ubloxI2c = new UbloxI2c(0x42);
    ubloxI2c->lock();
    if (ubloxI2c->devicePresent()) {
        if (verbose>2) {
            qInfo()<<"ublox I2C device interface is present.";
            uint16_t bufcnt = 0;
            bool ok = ubloxI2c->getTxBufCount(bufcnt);
            if (ok) qDebug()<<"bytes in TX buf: "<<bufcnt;

            //unsigned long int argh=0;
            //while (argh++<100UL) {
                //bufcnt = 0;
                //ok = ubloxI2c->getTxBufCount(bufcnt);
                //if (ok) cout<<"bytes in TX buf: "<<hex<<bufcnt<<dec<<endl;
                //std::string str=ubloxI2c->getData();
                //cout<<"string length: "<<str.size()<<endl;
                //usleep(200000L);
            //}
            //ubloxI2c->getData();
            //ok = ubloxI2c->getTxBufCount(bufcnt);
            //if (ok) cout<<"bytes in TX buf: "<<bufcnt<<endl;

        }
    } else {
        //cout<<"ublox I2C device interface NOT present"<<endl;
    }

    // check if also an Adafruit-SSD1306 compatible i2c OLED display is present
    // initialize and start loop for display of several state variables
    oled = new Adafruit_SSD1306(0x3c);
    if (oled->devicePresent()) {
        if (verbose>-1) {
            qInfo()<<"I2C SSD1306-type OLED display found at address 0x3c";
        }
        oled->begin();
        oled->clearDisplay();

        // text display tests
        oled->setTextSize(1);
        oled->setTextColor(Adafruit_SSD1306::WHITE);
        oled->setCursor(0,2);
        oled->print("*Cosmic Shower Det.*\n");
        oled->print("V");
        oled->print(MuonPi::Version::software.string().c_str());
        oled->print("\n");
        //oled->print("V 1.2.0\n");
        //  display.setTextColor(BLACK, WHITE); // 'inverted' text
/*
        struct timespec tNow;
        clock_gettime(CLOCK_REALTIME, &tNow);

        oled->printf("time:\n%ld.%06d\n", tNow.tv_sec, tNow.tv_nsec/1000L);
        oled->printf("  %02d s", tNow.tv_sec%60);
        oled->drawHorizontalBargraph(0,24,128,8,1, (tNow.tv_sec%60)*10/6);
*/
        //  display.setTextSize(1);
        //  display.setTextColor(WHITE);
        //  display.printf("0x%8X\n", 0xDEADBEEF);
        oled->display();
        usleep(500000L);
        connect(&oledUpdateTimer, SIGNAL(timeout()), this, SLOT(updateOledDisplay()));

        oledUpdateTimer.start(MuonPi::Config::Hardware::OLED::update_interval);
    } else {
        //cout<<"I2C OLED display NOT present"<<endl;
    }

    // for pigpio signals:
    preampStatus[0]=preamp1;
    preampStatus[1]=preamp2;
    gainSwitch=gain;
    biasON = bias_ON;
    eventTrigger = (GPIO_SIGNAL)new_eventTrigger;
//    eventTrigger = UNDEFINED_SIGNAL;
    polarity1=new_polarity1;
    polarity2=new_polarity2;


    // for diagnostics:
    // print out some i2c device statistics
    if (verbose>3) {
        cout<<"Nr. of invoked I2C devices (plain count): "<<i2cDevice::getNrDevices()<<endl;
        cout<<"Nr. of invoked I2C devices (gl. device list's size): "<<i2cDevice::getGlobalDeviceList().size()<<endl;
        cout<<"Nr. of bytes read on I2C bus: "<<i2cDevice::getGlobalNrBytesRead()<<endl;
        cout<<"Nr. of bytes written on I2C bus: "<<i2cDevice::getGlobalNrBytesWritten()<<endl;
        cout<<"list of device addresses: "<<endl;
        for (uint8_t i=0; i<i2cDevice::getGlobalDeviceList().size(); i++)
        {
            cout<<(int)i+1<<" 0x"<<hex<<(int)i2cDevice::getGlobalDeviceList()[i]->getAddress()<<" "<<i2cDevice::getGlobalDeviceList()[i]->getTitle();
            if (i2cDevice::getGlobalDeviceList()[i]->devicePresent()) cout<<" present"<<endl;
            else cout<<" missing"<<endl;
        }
        lm75->getCapabilities();
    }

    // for ublox gnss module
    gpsdevname = new_gpsdevname;
    dumpRaw = new_dumpRaw;
    baudrate = new_baudrate;
    configGnss = new_configGnss;
    showout = new_showout;
    showin = new_showin;

    // for tcp connection with fileServer
    peerPort = new_peerPort;
    if (peerPort == 0) {
        peerPort = 51508;
    }
    peerAddress = new_peerAddress;
    if (peerAddress.isEmpty() || peerAddress == "local" || peerAddress == "localhost") {
        peerAddress = QHostAddress(QHostAddress::LocalHost).toString();
    }

    if (new_daemonAddress.isEmpty()) {
        // if not otherwise specified: listen on all available addresses
        daemonAddress = QHostAddress(QHostAddress::Any);
        if (verbose > 3) {
            qDebug() << "daemon address: "<<daemonAddress.toString();
        }
    }
    daemonPort = new_daemonPort;
    if (daemonPort == 0) {
        // maybe think about other fall back solution
        daemonPort = 51508;
    }
    if (!this->listen(daemonAddress, daemonPort)) {
        qCritical() << tr("Unable to start the server: %1.\n").arg(this->errorString());
    }
    else {
        if (verbose > 3) {
            qInfo() << tr("\nThe server is running on\n\nIP: %1\nport: %2\n")
                .arg(daemonAddress.toString()).arg(serverPort());
        }
    }
    flush(cout);

	// set up histograms
    setupHistos();

	// connect to the pigpio daemon interface for gpio control
    connectToPigpiod();

	// set up the rate buffer for all GPIO signals
	rateBuffer.clear();
	connect(pigHandler, &PigpiodHandler::event, &rateBuffer, &RateBuffer::onEvent);
	connect(&rateBuffer, &RateBuffer::filteredEvent, this, &Daemon::sendGpioPinEvent);
	connect(&rateBuffer, &RateBuffer::filteredEvent, this, &Daemon::onGpioPinEvent);

	connect(this, &Daemon::eventInterval, this, [this](quint64 nsecs)
    { 	if (histoMap.find("gpioEventInterval")!=histoMap.end()) {
            //checkRescaleHisto(histoMap["gpioEventInterval"], 1e-6*nsecs);
            histoMap["gpioEventInterval"].fill(1e-6*nsecs);
        }
    } );

    connect(this, &Daemon::eventInterval, this, [this](quint64 nsecs)
    { 	if (histoMap.find("gpioEventIntervalShort")!=histoMap.end()) {
            if (nsecs/1000<=histoMap["gpioEventIntervalShort"].getMax())
                histoMap["gpioEventIntervalShort"].fill((double)nsecs/1000.);
        }
    } );

	// establish ublox gnss module connection
    connectToGps();
    //delay(1000);

    // configure the ublox module with preset ubx messages, if required
    if (configGnss) {
        configGps();
    }
    pollAllUbxMsgRate();

    // set up cyclic timer monitoring following operational parameters:
    // temp, vadc, vbias, ibias
    parameterMonitorTimer.setInterval(MuonPi::Config::Hardware::monitor_interval);
    parameterMonitorTimer.setSingleShot(false);
    connect(&parameterMonitorTimer, &QTimer::timeout, this, &Daemon::aquireMonitoringParameters);
    parameterMonitorTimer.start();
}


Daemon::~Daemon() {
    snHup.clear();
    snTerm.clear();
    snInt.clear();
    if (pca!=nullptr){ delete pca; pca = nullptr; }
    if (dac!=nullptr){ delete dac; dac = nullptr; }
    if (adc!=nullptr){ delete adc; adc = nullptr; }
    if (eep!=nullptr){ delete eep; eep = nullptr; }
    if (calib!=nullptr){ delete calib; calib = nullptr; }
    if (ubloxI2c!=nullptr){ delete ubloxI2c; ubloxI2c = nullptr; }
    if (oled!=nullptr){ delete oled; oled = nullptr; }
    pigHandler.clear();
    unsigned long timeout = 2000;
    if (!mqttHandlerThread->wait(timeout)){
        qWarning() << "Timeout waiting for thread "+mqttHandlerThread->objectName();
    }
    if (!fileHandlerThread->wait(timeout)){
        qWarning() << "Timeout waiting for thread "+fileHandlerThread->objectName();
    }
    if (!pigThread->wait(timeout)){
        qWarning() << "Timeout waiting for thread "+pigThread->objectName();
    }
    if (!gpsThread->wait(timeout)){
        qWarning() << "Timeout waiting for thread "+gpsThread->objectName();
    }
    if (!tcpThread->wait(timeout)){
        qWarning() << "Timeout waiting for thread "+tcpThread->objectName();
    }
}

void Daemon::connectToPigpiod(){
/*
	const QVector<unsigned int> gpio_pins({ GPIO_PINMAP[EVT_AND], GPIO_PINMAP[EVT_XOR],
                                            GPIO_PINMAP[TIMEPULSE], GPIO_PINMAP[EXT_TRIGGER] });
*/
	const std::vector<unsigned int> gpio_pins({ GPIO_PINMAP[EVT_AND], GPIO_PINMAP[EVT_XOR],
                                            GPIO_PINMAP[TIMEPULSE] });
    pigHandler = new PigpiodHandler(gpio_pins);
    //tdc7200 = new TDC7200(GPIO_PINMAP[TDC_INTB]);
    pigThread = new QThread();
    pigThread->setObjectName("muondetector-daemon-pigpio");
    pigHandler->moveToThread(pigThread);
    //tdc7200->moveToThread(pigThread);
    connect(this, &Daemon::aboutToQuit, pigThread, &QThread::quit);
    connect(pigThread, &QThread::finished, pigThread, &QThread::deleteLater);

    //pighandler <-> tdc
/*
	connect(pigHandler, &PigpiodHandler::spiData, tdc7200, &TDC7200::onDataReceived);
    connect(pigHandler, &PigpiodHandler::signal, tdc7200, &TDC7200::onDataAvailable);
    connect(tdc7200, &TDC7200::readData, pigHandler, &PigpiodHandler::readSpi);
    connect(tdc7200, &TDC7200::writeData, pigHandler, &PigpiodHandler::writeSpi);

    //tdc <-> thread & daemon
    connect(tdc7200, &TDC7200::tdcEvent, this, [this](double usecs){
        if (histoMap.find("Time-to-Digital Time Diff")!=histoMap.end()) {
            checkRescaleHisto(histoMap["Time-to-Digital Time Diff"],usecs);
            histoMap["Time-to-Digital Time Diff"].fill(usecs);
        }
    });
    connect(tdc7200, &TDC7200::statusUpdated, this, [this](bool isPresent){
       spiDevicePresent = isPresent;
       sendSpiStats();
    });
    connect(pigThread, &QThread::started, tdc7200, &TDC7200::initialise);
    connect(pigThread, &QThread::finished, tdc7200, &TDC7200::deleteLater);
*/
    //pigHandler <-> thread & daemon
    connect(this, &Daemon::aboutToQuit, pigHandler, &PigpiodHandler::stop);
    connect(pigThread, &QThread::finished, pigHandler, &PigpiodHandler::deleteLater);
    connect(this, &Daemon::GpioSetOutput, pigHandler, &PigpiodHandler::setPinOutput);
    connect(this, &Daemon::GpioSetInput, pigHandler, &PigpiodHandler::setPinInput);
///    connect(this, &Daemon::GpioSetPullUp, pigHandler, &PigpiodHandler::setPullUp);
///    connect(this, &Daemon::GpioSetPullDown, pigHandler, &PigpiodHandler::setPullDown);
    connect(this, &Daemon::GpioSetState, pigHandler, &PigpiodHandler::setPinState);
    connect(this, &Daemon::GpioRegisterForCallback, pigHandler, &PigpiodHandler::registerInterrupt);
//    connect(pigHandler, &PigpiodHandler::signal, this, &Daemon::onGpioPinEvent);
//    connect(pigHandler, &PigpiodHandler::timePulseDiff, this, [this](qint32 usecs)

    struct timespec ts_res;
    clock_getres(CLOCK_REALTIME, &ts_res);
    if (verbose>3) {
        qInfo()<<"the timing resolution of the system clock is "<<ts_res.tv_nsec<<" ns";
    }

    /* looks good but using QPointer should be safer
     * connect(pigHandler, &PigpiodHandler::destroyed, this, [this](){pigHandler = nullptr;});
    */
    // test if "aboutToQuit" is called everytime
    //connect(this, &Daemon::aboutToQuit, [this](){this->toConsole("aboutToQuit called and signal emitted\n");});
    timespec_get(&lastRateInterval, TIME_UTC);
    startOfProgram = lastRateInterval;

// TODO: delete following block
/*
	connect(pigHandler, &PigpiodHandler::signal, this, [this](uint8_t gpio_pin){
//    connect(&rateBuffer, &RateBuffer::throttledSignal, this, [this](uint8_t gpio_pin){
        rateCounterIntervalActualisation();
        if (gpio_pin==GPIO_PINMAP[EVT_XOR]){
            xorCounts.back()++;
            //xorCounts.pop_back();
            //value++;
            //xorCounts.push_back(value);
        }
        if (gpio_pin==GPIO_PINMAP[EVT_AND]){
            //quint64 value = andCounts.back();
            andCounts.back()++;
            //andCounts.pop_back();
            //value++;
            //andCounts.push_back(value);
        }
    });
    */
    pigThread->start();
    rateBufferReminder.setInterval(rateBufferInterval);
    rateBufferReminder.setSingleShot(false);
    connect(&rateBufferReminder, &QTimer::timeout, this, &Daemon::onRateBufferReminder);
    rateBufferReminder.start();
    emit GpioSetOutput(GPIO_PINMAP[UBIAS_EN]);
    emit GpioSetState(GPIO_PINMAP[UBIAS_EN], (HW_VERSION==1)?(biasON?1:0):(biasON?0:1));
    emit GpioSetOutput(GPIO_PINMAP[PREAMP_1]);
    emit GpioSetOutput(GPIO_PINMAP[PREAMP_2]);
    emit GpioSetOutput(GPIO_PINMAP[GAIN_HL]);
    emit GpioSetState(GPIO_PINMAP[PREAMP_1],preampStatus[0]);
    emit GpioSetState(GPIO_PINMAP[PREAMP_2],preampStatus[1]);
    emit GpioSetState(GPIO_PINMAP[GAIN_HL],gainSwitch);
    emit GpioSetOutput(GPIO_PINMAP[STATUS1]);
    emit GpioSetOutput(GPIO_PINMAP[STATUS2]);
    emit GpioSetState(GPIO_PINMAP[STATUS1], 0);
    emit GpioSetState(GPIO_PINMAP[STATUS2], 0);
    emit GpioSetPullUp(GPIO_PINMAP[ADC_READY]);
    emit GpioRegisterForCallback(GPIO_PINMAP[ADC_READY], PigpiodHandler::EventEdge::RisingEdge);
	auto it=GPIO_PINMAP.find(eventTrigger);
	if (it != GPIO_PINMAP.end()) {
//		pigHandler->setSamplingTriggerSignal( GPIO_PINMAP[eventTrigger] );
		emit GpioRegisterForCallback(GPIO_PINMAP[eventTrigger], PigpiodHandler::EventEdge::RisingEdge);
	}
//    connect(this, &Daemon::setSamplingTriggerSignal, pigHandler, &PigpiodHandler::setSamplingTriggerSignal);

    if (HW_VERSION>1) {
        //emit GpioSetInput(GPIO_PINMAP[PREAMP_FAULT]);
        emit GpioRegisterForCallback(GPIO_PINMAP[PREAMP_FAULT], PigpiodHandler::EventEdge::FallingEdge);
        emit GpioSetPullUp(GPIO_PINMAP[PREAMP_FAULT]);
        //emit GpioSetInput(GPIO_PINMAP[TDC_INTB]);
        emit GpioRegisterForCallback(GPIO_PINMAP[TDC_INTB], PigpiodHandler::EventEdge::FallingEdge);
        //emit GpioSetInput(GPIO_PINMAP[TIME_MEAS_OUT]);
        emit GpioRegisterForCallback(GPIO_PINMAP[TIME_MEAS_OUT], PigpiodHandler::EventEdge::FallingEdge);
    }
    if (HW_VERSION>=3) {
        emit GpioSetOutput(GPIO_PINMAP[IN_POL1]);
        emit GpioSetOutput(GPIO_PINMAP[IN_POL2]);
        emit GpioSetState(GPIO_PINMAP[IN_POL1],polarity1);
        emit GpioSetState(GPIO_PINMAP[IN_POL2],polarity2);
    }

}

void Daemon::connectToGps() {
    // before connecting to gps we have to make sure all other programs are closed
    // and serial echo is off
    if (gpsdevname.isEmpty()) {
        return;
    }
    QProcess prepareSerial;
    QString command = "stty";
    QStringList args = { "-F", "/dev/ttyAMA0", "-echo", "-onlcr" };
    prepareSerial.start(command, args, QIODevice::ReadWrite);
    prepareSerial.waitForFinished();

    // here is where the magic threading happens look closely
    qtGps = new QtSerialUblox(gpsdevname, gpsTimeout, baudrate, dumpRaw, verbose-1, showout, showin);
    gpsThread = new QThread();
    gpsThread->setObjectName("muondetector-daemon-gnss");
    qtGps->moveToThread(gpsThread);
    // connect all signals about quitting
    connect(this, &Daemon::aboutToQuit, gpsThread, &QThread::quit);
    connect(gpsThread, &QThread::finished, gpsThread, &QThread::deleteLater);
    connect(gpsThread, &QThread::finished, qtGps, &QtSerialUblox::deleteLater);
    // connect all signals not coming from Daemon to gps
    connect(qtGps, &QtSerialUblox::toConsole, this, &Daemon::gpsToConsole);
    connect(gpsThread, &QThread::started, qtGps, &QtSerialUblox::makeConnection);
    connect(qtGps, &QtSerialUblox::gpsRestart, this, &Daemon::connectToGps);
    // connect all command signals for ublox module here
    connect(this, &Daemon::UBXSetCfgPrt, qtGps, &QtSerialUblox::UBXSetCfgPrt);
    connect(this, &Daemon::UBXSetCfgMsgRate, qtGps, &QtSerialUblox::UBXSetCfgMsgRate);
    connect(this, &Daemon::UBXSetCfgRate, qtGps, &QtSerialUblox::UBXSetCfgRate);
    connect(this, &Daemon::sendPollUbxMsgRate, qtGps, &QtSerialUblox::pollMsgRate);
    connect(this, &Daemon::sendPollUbxMsg, qtGps, &QtSerialUblox::pollMsg);
    connect(this, &Daemon::sendUbxMsg, qtGps, &QtSerialUblox::enqueueMsg);
    connect(qtGps, &QtSerialUblox::UBXReceivedAckNak, this, &Daemon::UBXReceivedAckNak);
    connect(qtGps, &QtSerialUblox::UBXreceivedMsgRateCfg, this, &Daemon::UBXReceivedMsgRateCfg);
    connect(qtGps, &QtSerialUblox::gpsPropertyUpdatedGeodeticPos, this, &Daemon::onGpsPropertyUpdatedGeodeticPos);
    connect(qtGps, &QtSerialUblox::gpsPropertyUpdatedGnss, this, &Daemon::onGpsPropertyUpdatedGnss);
    connect(qtGps, &QtSerialUblox::gpsPropertyUpdatedUint32, this, &Daemon::gpsPropertyUpdatedUint32);
    connect(qtGps, &QtSerialUblox::gpsPropertyUpdatedInt32, this, &Daemon::gpsPropertyUpdatedInt32);
    connect(qtGps, &QtSerialUblox::gpsPropertyUpdatedUint8, this, &Daemon::gpsPropertyUpdatedUint8);
    connect(qtGps, &QtSerialUblox::gpsMonHW, this, &Daemon::onGpsMonHWUpdated);
    connect(qtGps, &QtSerialUblox::gpsMonHW2, this, &Daemon::onGpsMonHW2Updated);
    connect(qtGps, &QtSerialUblox::gpsVersion, this, &Daemon::UBXReceivedVersion);
    connect(qtGps, &QtSerialUblox::UBXCfgError, this, &Daemon::toConsole);
    connect(qtGps, &QtSerialUblox::UBXReceivedGnssConfig, this, &Daemon::onUBXReceivedGnssConfig);
    connect(qtGps, &QtSerialUblox::UBXReceivedTP5, this, &Daemon::onUBXReceivedTP5);
    connect(qtGps, &QtSerialUblox::UBXReceivedTxBuf, this, &Daemon::onUBXReceivedTxBuf);
    connect(qtGps, &QtSerialUblox::UBXReceivedRxBuf, this, &Daemon::onUBXReceivedRxBuf);
    connect(this, &Daemon::UBXSetDynModel, qtGps, &QtSerialUblox::setDynamicModel);
    connect(this, &Daemon::resetUbxDevice, qtGps, &QtSerialUblox::UBXReset);
    connect(this, &Daemon::setGnssConfig, qtGps, &QtSerialUblox::onSetGnssConfig);
    connect(this, &Daemon::UBXSetCfgTP5, qtGps, &QtSerialUblox::UBXSetCfgTP5);
    connect(this, &Daemon::UBXSetMinMaxSVs, qtGps, &QtSerialUblox::UBXSetMinMaxSVs);
    connect(this, &Daemon::UBXSetMinCNO, qtGps, &QtSerialUblox::UBXSetMinCNO);
    connect(this, &Daemon::UBXSetAopCfg, qtGps, &QtSerialUblox::UBXSetAopCfg);
    connect(this, &Daemon::UBXSaveCfg, qtGps, &QtSerialUblox::UBXSaveCfg);
    connect(qtGps, &QtSerialUblox::UBXReceivedTimeTM2, this, &Daemon::onUBXReceivedTimeTM2);

    connect(qtGps, &QtSerialUblox::UBXReceivedDops, this, [this](const UbxDopStruct& dops){
        currentDOP=dops;
        emit logParameter(LogParameter("positionDOP", QString::number(dops.pDOP/100.), LogParameter::LOG_AVERAGE));
        emit logParameter(LogParameter("timeDOP", QString::number(dops.tDOP/100.), LogParameter::LOG_AVERAGE));
    });

    // connect fileHandler related stuff
    if (fileHandler != nullptr){
        //connect(qtGps, &QtSerialUblox::timTM2, fileHandler, &FileHandler::writeToDataFile);
        connect(this, &Daemon::eventMessage, fileHandler, &FileHandler::writeToDataFile);
		//connect(qtGps, &QtSerialUblox::timTM2, mqttHandler, &MuonPi::MqttHandler::sendData);
		connect(this, &Daemon::eventMessage, mqttHandler, &MuonPi::MqttHandler::sendData);
    }
    // after thread start there will be a signal emitted which starts the qtGps makeConnection function
    gpsThread->start();
}

void Daemon::incomingConnection(qintptr socketDescriptor) {
    if (verbose > 4) {
        qDebug() << "incoming connection";
    }
    tcpThread = new QThread();
    tcpThread->setObjectName("muondetector-daemon-tcp");
//    TcpConnection *tcpConnection = new TcpConnection(socketDescriptor, verbose);
    tcpConnection = new TcpConnection(socketDescriptor, verbose);
    tcpConnection->moveToThread(tcpThread);
    // connect all signals about quitting
    connect(this, &Daemon::aboutToQuit, tcpConnection, &TcpConnection::closeThisConnection);
    connect(this, &Daemon::closeConnection, tcpConnection, &TcpConnection::closeConnection);
    connect(tcpConnection, &TcpConnection::finished, tcpThread, &QThread::quit);
    connect(tcpThread, &QThread::finished, tcpThread, &QThread::deleteLater);
    connect(tcpThread, &QThread::finished, tcpConnection, &TcpConnection::deleteLater);
    // connect all other signals
    connect(tcpThread, &QThread::started, tcpConnection, &TcpConnection::receiveConnection);
    connect(this, &Daemon::sendTcpMessage, tcpConnection, &TcpConnection::sendTcpMessage);
    connect(tcpConnection, &TcpConnection::receivedTcpMessage, this, &Daemon::receivedTcpMessage);
    connect(tcpConnection, &TcpConnection::toConsole, this, &Daemon::toConsole);
//	connect(tcpConnection, &TcpConnection::madeConnection, this, [this](QString, quint16, QString , quint16) { emit sendPollUbxMsg(UBX_MON_VER); });
    connect(tcpConnection, &TcpConnection::madeConnection, this, &Daemon::onMadeConnection);
    connect(tcpConnection, &TcpConnection::connectionTimeout, this, &Daemon::onStoppedConnection);
    tcpThread->start();

/*
    emit sendPollUbxMsg(UBX_MON_VER);
    emit sendPollUbxMsg(UBX_CFG_GNSS);
    emit sendPollUbxMsg(UBX_CFG_NAV5);
    emit sendPollUbxMsg(UBX_CFG_TP5);
    emit sendPollUbxMsg(UBX_CFG_NAVX5);

    sendBiasStatus();
    sendBiasVoltage();
    sendDacThresh(0);
    sendDacThresh(1);
    sendPcaChannel();
    sendEventTriggerSelection();
*/

    pollAllUbxMsgRate();
    emit requestMqttConnectionStatus();
}

// Histogram functions
void Daemon::setupHistos() {
    Histogram hist=Histogram("geoHeight",200,0.,199.);
    hist.setUnit("m");
    histoMap["geoHeight"]=hist;
    hist=Histogram("geoLongitude",200,0.,0.003);
    hist.setUnit("deg");
    histoMap["geoLongitude"]=hist;
    hist=Histogram("geoLatitude",200,0.,0.003);
    hist.setUnit("deg");
    histoMap["geoLatitude"]=hist;
    hist=Histogram("weightedGeoHeight",200,0.,199.);
    hist.setUnit("m");
    histoMap["weightedGeoHeight"]=hist;
    hist=Histogram("pulseHeight",500,0.,3.8);
    hist.setUnit("V");
    histoMap["pulseHeight"]=hist;
    hist=Histogram("adcSampleTime",500,0.,49.9);
    hist.setUnit("ms");
    histoMap["adcSampleTime"]=hist;
    hist=Histogram("UbxEventLength",100,50.,149.);
    hist.setUnit("ns");
    histoMap["UbxEventLength"]=hist;
    hist=Histogram("gpioEventInterval",400,0.,2000.);
    hist.setUnit("ms");
    histoMap["gpioEventInterval"]=hist;
    hist=Histogram("gpioEventIntervalShort",50,0.,49.);
    hist.setUnit("us");
    histoMap["gpioEventIntervalShort"]=hist;
    hist=Histogram("UbxEventInterval",400,0.,2000.);
    hist.setUnit("ms");
    histoMap["UbxEventInterval"]=hist;
    hist=Histogram("TPTimeDiff",200,-999.,1000.);
    hist.setUnit("us");
    histoMap["TPTimeDiff"]=hist;
    hist=Histogram("Time-to-Digital Time Diff",400, 0., 1e6);
    hist.setUnit("ns");
    histoMap["Time-to-Digital Time Diff"]=hist;
    hist=Histogram("Bias Voltage", 500, 0., 1.);
    hist.setUnit("V");
    histoMap["Bias Voltage"]=hist;
    hist=Histogram("Bias Current", 200, 0., 50.);
    hist.setUnit("uA");
    histoMap["Bias Current"]=hist;
}

void Daemon::clearHisto(const QString& histoName){
    if (histoMap.find(histoName)!=histoMap.end()) {
        histoMap[histoName].clear();
        emit sendHistogram(histoMap[histoName]);
    }
    return;
}

void Daemon::rescaleHisto(Histogram& hist, double center, double width) {
    hist.setMin(center-width/2.);
    hist.setMax(center+width/2.);
    hist.clear();
}

void Daemon::rescaleHisto(Histogram& hist, double center) {
    double width=hist.getMax()-hist.getMin();
    rescaleHisto(hist, center, width);
}

void Daemon::checkRescaleHisto(Histogram& hist, double newValue) {
    // Strategy: check if more than 10% of all entries in underflow/overflow
    // set new center to old mean
    // set center to newValue if histo empty or only underflow/overflow filled
    // histo will not be filled with supplied value, it has to be done externally
    double entries=hist.getEntries();
    // do nothing if histo is empty
    if (entries<3.) {
        return;
        rescaleHisto(hist, newValue);
    }
    double ufl=hist.getUnderflow();
    double ofl=hist.getOverflow();
    entries-=ufl+ofl;
    double range=hist.getMax()-hist.getMin();
    if (ufl>0. && ofl>0. && (ufl+ofl)>0.02*entries) {
        // range is too small, underflow and overflow have more than 2% of all entries
        rescaleHisto(hist, hist.getMean(), 1.2*range);
    } else if (ufl>0.02*entries) {
//		if (entries<1.) rescaleHisto(hist, hist.getMin()-(hist.getMax()-hist.getMin())/2.);
        if (entries<1.) rescaleHisto(hist, newValue);
        else rescaleHisto(hist, hist.getMean(), 1.2*range);
    } else if (ofl>0.02*entries) {
//		if (entries<1.) rescaleHisto(hist, hist.getMax()+(hist.getMax()-hist.getMin())/2.);
        if (entries<1.) rescaleHisto(hist, newValue);
        else rescaleHisto(hist, hist.getMean(), 1.2*range);
    } else if (ufl<1e-3 && ofl<1e-3) {
        // check if range is too wide
        if (entries>100) {
            int lowest = hist.getLowestOccupiedBin();
            int highest = hist.getHighestOccupiedBin();
            if ((float)lowest/hist.getNrBins()>0.45 && (float)(hist.getNrBins()-highest)/hist.getNrBins()>0.45) rescaleHisto(hist, hist.getMean(), 0.75*range);
        }
    }
}

// ALL FUNCTIONS ABOUT TCPMESSAGE SENDING AND RECEIVING
void Daemon::receivedTcpMessage(TcpMessage tcpMessage) {
//    quint16 msgID = tcpMessage.getMsgID();
    TCP_MSG_KEY msgID = static_cast<TCP_MSG_KEY>(tcpMessage.getMsgID());
    if (msgID == TCP_MSG_KEY::MSG_THRESHOLD) {
        uint8_t channel;
        float threshold;
        *(tcpMessage.dStream) >> channel >> threshold;
        if (threshold<0.001) {
            if (verbose>2) qWarning()<<"setting DAC "<<channel<<" to 0!";
        } else setDacThresh(channel, threshold);
        sendDacThresh(DAC_TH1);
        sendDacThresh(DAC_TH2);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_THRESHOLD_REQUEST){
        sendDacThresh(DAC_TH1);
        sendDacThresh(DAC_TH2);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_VOLTAGE){
        float voltage;
        *(tcpMessage.dStream) >> voltage;
        setBiasVoltage(voltage);
        if (histoMap.find("pulseHeight")!=histoMap.end()) histoMap["pulseHeight"].clear();
        sendBiasVoltage();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_VOLTAGE_REQUEST){
        sendBiasVoltage();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_SWITCH){
        bool status;
        *(tcpMessage.dStream) >> status;
        setBiasStatus(status);
        //pulseHeightHisto.clear();
        sendBiasStatus();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_BIAS_VOLTAGE_REQUEST){
        sendBiasStatus();
    }
    if (msgID == TCP_MSG_KEY::MSG_PREAMP_SWITCH){
        quint8 channel;
        bool status;
        *(tcpMessage.dStream) >> channel >> status;
        if (channel==0) {
            preampStatus[0]=status;
            emit GpioSetState(GPIO_PINMAP[PREAMP_1], status);
            emit logParameter(LogParameter("preampSwitch1", QString::number((int)preampStatus[0]), LogParameter::LOG_EVERY));
            //pulseHeightHisto.clear();
        } else if (channel==1) {
            preampStatus[1]=status;
            emit GpioSetState(GPIO_PINMAP[PREAMP_2], status);
            emit logParameter(LogParameter("preampSwitch2", QString::number((int)preampStatus[1]), LogParameter::LOG_EVERY));
        }
        sendPreampStatus(0);
        sendPreampStatus(1);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_PREAMP_SWITCH_REQUEST){
        sendPreampStatus(0);
        sendPreampStatus(1);
    }
    if (msgID == TCP_MSG_KEY::MSG_POLARITY_SWITCH){
        bool pol1,pol2;
        *(tcpMessage.dStream) >> pol1 >> pol2;
        if (HW_VERSION>=3 && pol1 != polarity1) {
            polarity1=pol1;
            emit GpioSetState(GPIO_PINMAP[IN_POL1], polarity1);
            emit logParameter(LogParameter("polaritySwitch1", QString::number((int)polarity1), LogParameter::LOG_EVERY));
        }
        if (HW_VERSION>=3 && pol2 != polarity2) {
            polarity2=pol2;
            emit GpioSetState(GPIO_PINMAP[IN_POL2], polarity2);
            emit logParameter(LogParameter("polaritySwitch2", QString::number((int)polarity2), LogParameter::LOG_EVERY));
        }
        sendPolarityStatus();
    }
    if (msgID == TCP_MSG_KEY::MSG_POLARITY_SWITCH_REQUEST){
        sendPolarityStatus();
    }
    if (msgID == TCP_MSG_KEY::MSG_GAIN_SWITCH){
        bool status;
        *(tcpMessage.dStream) >> status;
        gainSwitch=status;
        emit GpioSetState(GPIO_PINMAP[GAIN_HL], status);
        if (histoMap.find("pulseHeight")!=histoMap.end()) histoMap["pulseHeight"].clear();
        //pulseHeightHisto.clear();
        emit logParameter(LogParameter("gainSwitch", QString::number((int)gainSwitch), LogParameter::LOG_EVERY));
        sendGainSwitchStatus();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_GAIN_SWITCH_REQUEST){
        sendGainSwitchStatus();
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_MSG_RATE_REQUEST) {
//    if (msgID == ubxMsgRateRequest) {
        sendUbxMsgRates();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_RESET) {
        uint32_t resetFlags = QtSerialUblox::RESET_WARM | QtSerialUblox::RESET_SW;
        emit resetUbxDevice(resetFlags);
        pollAllUbxMsgRate();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_CONFIG_DEFAULT) {
        configGps();
        pollAllUbxMsgRate();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_MSG_RATE){
        QMap<uint16_t, int> ubxMsgRates;
        *(tcpMessage.dStream) >> ubxMsgRates;
        setUbxMsgRates(ubxMsgRates);
    }
    if (msgID == TCP_MSG_KEY::MSG_PCA_SWITCH){
        quint8 portMask;
        *(tcpMessage.dStream) >> portMask;
        setPcaChannel((uint8_t)portMask);
        sendPcaChannel();
//        ubxTimeLengthHisto.clear();
        if (histoMap.find("UbxEventLength")!=histoMap.end()) histoMap["UbxEventLength"].clear();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_PCA_SWITCH_REQUEST){
        sendPcaChannel();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_EVENTTRIGGER){
        unsigned int signal;
        *(tcpMessage.dStream) >> signal;
        setEventTriggerSelection((GPIO_SIGNAL)signal);
        usleep(1000);
        sendEventTriggerSelection();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_EVENTTRIGGER_REQUEST){
        sendEventTriggerSelection();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_GPIO_RATE_REQUEST){
        quint8 whichRate;
        quint16 number;
        *(tcpMessage.dStream) >> number >> whichRate;
        sendGpioRates(number, whichRate);
    }
    if (msgID == TCP_MSG_KEY::MSG_GPIO_RATE_RESET){
        clearRates();
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_DAC_REQUEST){
        quint8 channel;
        *(tcpMessage.dStream) >> channel;
        MCP4728::DacChannel channelData;
        if (!dac->devicePresent()) return;
        dac->readChannel(channel, channelData);
        float voltage = MCP4728::code2voltage(channelData);
        sendDacReadbackValue(channel, voltage);
    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_SAMPLE_REQUEST){
        quint8 channel;
        *(tcpMessage.dStream) >> channel;
        sampleAdcEvent(channel);
    }
    if (msgID == TCP_MSG_KEY::MSG_TEMPERATURE_REQUEST){
        getTemperature();
    }
    if (msgID == TCP_MSG_KEY::MSG_I2C_STATS_REQUEST){
        sendI2cStats();
    }
        if (msgID == TCP_MSG_KEY::MSG_I2C_SCAN_BUS){
        scanI2cBus();
        sendI2cStats();
    }
    if (msgID == TCP_MSG_KEY::MSG_SPI_STATS_REQUEST){
        sendSpiStats();
    }
    if (msgID == TCP_MSG_KEY::MSG_CALIB_REQUEST){
        sendCalib();
    }
    if (msgID == TCP_MSG_KEY::MSG_CALIB_SAVE){
        if (calib!=nullptr) calib->writeToEeprom();
        sendCalib();
    }
    if (msgID == TCP_MSG_KEY::MSG_CALIB_SET){
        std::vector<CalibStruct> calibs;
        quint8 nrEntries=0;
        *(tcpMessage.dStream) >> nrEntries;
        for (int i=0; i<nrEntries; i++) {
            CalibStruct item;
            *(tcpMessage.dStream) >> item;
            calibs.push_back(item);
        }
        receivedCalibItems(calibs);
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_GNSS_CONFIG){
        std::vector<GnssConfigStruct> configs;
        int nrEntries=0;
        *(tcpMessage.dStream) >> nrEntries;
        for (int i=0; i<nrEntries; i++) {
            GnssConfigStruct config;
            *(tcpMessage.dStream) >> config.gnssId >> config.resTrkCh >> config.maxTrkCh >> config.flags;
            configs.push_back(config);
        }
        emit setGnssConfig(configs);
        usleep(150000L);
        emit sendPollUbxMsg(UBX_CFG_GNSS);
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_CFG_TP5){
        UbxTimePulseStruct tp;
        *(tcpMessage.dStream) >> tp;
        emit UBXSetCfgTP5(tp);
        emit sendPollUbxMsg(UBX_CFG_TP5);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_UBX_CFG_SAVE){
        emit UBXSaveCfg();
        emit sendPollUbxMsg(UBX_CFG_TP5);
        emit sendPollUbxMsg(UBX_CFG_GNSS);
        return;
    }
    if (msgID == TCP_MSG_KEY::MSG_QUIT_CONNECTION){
        QString closeAddress;
        *(tcpMessage.dStream) >> closeAddress;
        emit closeConnection(closeAddress);
    }
    if (msgID == TCP_MSG_KEY::MSG_DAC_EEPROM_SET){
        saveDacValuesToEeprom();
    }
    if (msgID == TCP_MSG_KEY::MSG_HISTOGRAM_CLEAR){
        QString histoName;
        *(tcpMessage.dStream) >> histoName;
        clearHisto(histoName);
    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_MODE_REQUEST){
        TcpMessage answer(TCP_MSG_KEY::MSG_ADC_MODE);
        *(answer.dStream) << (quint8)adcSamplingMode;
        emit sendTcpMessage(answer);

    }
    if (msgID == TCP_MSG_KEY::MSG_ADC_MODE){
        quint8 mode=0;
        *(tcpMessage.dStream) >> mode;
        setAdcSamplingMode(mode);
        TcpMessage answer(TCP_MSG_KEY::MSG_ADC_MODE);
        *(answer.dStream) << (quint8)adcSamplingMode;
        emit sendTcpMessage(answer);
    }
    if (msgID == TCP_MSG_KEY::MSG_LOG_INFO){
        sendLogInfo();
    }
    if (msgID == TCP_MSG_KEY::MSG_RATE_SCAN) {
        quint8 channel=0;
        *(tcpMessage.dStream) >> channel;
        startRateScan(channel);
    }
    if (msgID == TCP_MSG_KEY::MSG_GPIO_INHIBIT) {
        bool inhibit=true;
        *(tcpMessage.dStream) >> inhibit;
        if (pigHandler!=nullptr) pigHandler->setInhibited(inhibit);
    }
}

void Daemon::setAdcSamplingMode(quint8 mode) {
    if (mode>ADC_MODE_TRACE) return;
    adcSamplingMode=mode;
    if (mode==ADC_MODE_TRACE) samplingTimer.start();
    else samplingTimer.stop();
}

void Daemon::scanI2cBus() {
    for (uint8_t addr=1; addr<0x7f; addr++)
    {
        bool alreadyThere = false;
        for (uint8_t i=0; i<i2cDevice::getGlobalDeviceList().size(); i++) {
            if (addr==i2cDevice::getGlobalDeviceList()[i]->getAddress()) {
                alreadyThere=true;
                break;
            }
        }
        if (alreadyThere) continue;
        i2cDevice* dev = new i2cDevice(addr);
        if (!dev->devicePresent()) delete dev;
    }
}

void Daemon::sendLogInfo(){
    LogInfoStruct lis;
    lis.logFileName=fileHandler->logFileInfo().absoluteFilePath();
    lis.dataFileName=fileHandler->dataFileInfo().absoluteFilePath();
    lis.logFileSize=fileHandler->logFileInfo().size();
    lis.dataFileSize=fileHandler->dataFileInfo().size();
    lis.status=(quint8)(fileHandler->dataFileInfo().exists() && fileHandler->logFileInfo().exists());
// following code works only from Qt 5.10 onwards
/*
    QDateTime now = QDateTime::currentDateTime();
    qint64 difftime=-now.secsTo(fileHandler->dataFileInfo().fileTime(QFileDevice::FileMetadataChangeTime));
    lis.logAge=(qint32)difftime;
*/
    lis.logAge=(qint32)fileHandler->currentLogAge();
    TcpMessage answer(TCP_MSG_KEY::MSG_LOG_INFO);
    *(answer.dStream) << lis;
    emit sendTcpMessage(answer);
}

void Daemon::sendI2cStats() {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_I2C_STATS);
    quint8 nrDevices=i2cDevice::getGlobalDeviceList().size();
    quint32 bytesRead = i2cDevice::getGlobalNrBytesRead();
    quint32 bytesWritten = i2cDevice::getGlobalNrBytesWritten();
    *(tcpMessage.dStream) << nrDevices << bytesRead << bytesWritten;

    for (uint8_t i=0; i<i2cDevice::getGlobalDeviceList().size(); i++)
    {
        uint8_t addr = i2cDevice::getGlobalDeviceList()[i]->getAddress();
        QString title = QString::fromStdString(i2cDevice::getGlobalDeviceList()[i]->getTitle());
        i2cDevice::getGlobalDeviceList()[i]->devicePresent();
        uint8_t status = i2cDevice::getGlobalDeviceList()[i]->getStatus();
        *(tcpMessage.dStream) << addr << title << status;
    }
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendSpiStats() {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_SPI_STATS);
    *(tcpMessage.dStream) << spiDevicePresent;
    emit sendTcpMessage(spiDevicePresent);
}

void Daemon::sendCalib() {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_CALIB_SET);
    bool valid=calib->isValid();
    bool eepValid=calib->isEepromValid();
    quint16 nrPars = calib->getCalibList().size();
    quint64 id = calib->getSerialID();
    *(tcpMessage.dStream) << valid << eepValid << id << nrPars;
    for (int i=0; i<nrPars; i++) {
        *(tcpMessage.dStream) << calib->getCalibItem(i);
    }
    emit sendTcpMessage(tcpMessage);
}

void Daemon::receivedCalibItems(const std::vector<CalibStruct>& newCalibs) {
    for (unsigned int i=0; i<newCalibs.size(); i++) {
        calib->setCalibItem(newCalibs[i].name, newCalibs[i]);
    }
}


void Daemon::onGpsPropertyUpdatedGeodeticPos(const GeodeticPos& pos){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GEO_POS);
    (*tcpMessage.dStream) << pos.iTOW << pos.lon << pos.lat
                          << pos.height << pos.hMSL << pos.hAcc
                          << pos.vAcc;
    emit sendTcpMessage(tcpMessage);

    QString geohash=GeoHash::hashFromCoordinates(1e-7*pos.lon, 1e-7*pos.lat, 10);

    emit logParameter(LogParameter("geoLongitude", QString::number(1e-7*pos.lon,'f',7)+" deg", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("geoLatitude", QString::number(1e-7*pos.lat,'f',7)+" deg", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("geoHash", geohash+" ", LogParameter::LOG_LATEST));
    emit logParameter(LogParameter("geoHeightMSL", QString::number(1e-3*pos.hMSL,'f',2)+" m", LogParameter::LOG_AVERAGE));
    if (histoMap.find("geoHeight")!=histoMap.end())
        emit logParameter(LogParameter("meanGeoHeightMSL", QString::number(histoMap["geoHeight"].getMean(),'f',2)+" m", LogParameter::LOG_LATEST));
    emit logParameter(LogParameter("geoHorAccuracy", QString::number(1e-3*pos.hAcc,'f',2)+" m", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("geoVertAccuracy", QString::number(1e-3*pos.vAcc,'f',2)+" m", LogParameter::LOG_AVERAGE));

    if (1e-3*pos.vAcc<100.) {
        QString name="geoHeight";
        if (histoMap.find(name)!=histoMap.end()) {
            checkRescaleHisto(histoMap[name], 1e-3*pos.hMSL);
            histoMap[name].fill(1e-3*pos.hMSL /*, heightWeight */);
            if (currentDOP.vDOP>0) {
                name="weightedGeoHeight";
                double heightWeight=100./currentDOP.vDOP;
                checkRescaleHisto(histoMap[name], 1e-3*pos.hMSL);
                histoMap[name].fill(1e-3*pos.hMSL , heightWeight );
            }
        }
    }
    if (1e-3*pos.hAcc<100.) {
        QString name="geoLongitude";
        checkRescaleHisto(histoMap[name], 1e-7*pos.lon);
        histoMap[name].fill(1e-7*pos.lon /*, heightWeight */);
        name="geoLatitude";
        checkRescaleHisto(histoMap[name], 1e-7*pos.lat);
        histoMap[name].fill(1e-7*pos.lat /*, heightWeight */);
    }
}

void Daemon::sendHistogram(const Histogram& hist){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_HISTOGRAM);
    (*tcpMessage.dStream) << hist;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendUbxMsgRates() {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_MSG_RATE);
    *(tcpMessage.dStream) << msgRateCfgs;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendDacThresh(uint8_t channel) {
    if (channel > 1){ return; }
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_THRESHOLD);
    *(tcpMessage.dStream) << (quint8)channel << dacThresh[(int)channel];
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendDacReadbackValue(uint8_t channel, float voltage) {
    if (channel > 3){ return; }

    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_DAC_READBACK);
    *(tcpMessage.dStream) << (quint8)channel << voltage;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::onGpioPinEvent(unsigned int gpio, EventTime event_time) {
    // reverse lookup of gpio function from given pin (first occurence)
    auto result=std::find_if(GPIO_PINMAP.begin(), GPIO_PINMAP.end(), [&gpio](const std::pair<GPIO_SIGNAL,unsigned int>& item)
                    { return item.second == gpio; }
                    );
    if (result != GPIO_PINMAP.end()) {
		if ( result->first == TIMEPULSE) {
			auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(event_time.time_since_epoch()).count();
			usecs = usecs % 1000000LL;
			if ( usecs > 500000L ) {
				usecs -= 1000000LL;
			}
			if (histoMap.find("TPTimeDiff") != histoMap.end()) {
				checkRescaleHisto(histoMap["TPTimeDiff"], usecs);
				histoMap["TPTimeDiff"].fill((double)usecs);
			}
			if (verbose>2) qDebug()<<"TP time diff:"<<usecs<<"us";
		} else if ( result->first == EVT_XOR || result->first == EVT_AND ) {
	        rateCounterIntervalActualisation();
			if ( result->first == EVT_XOR ) {
				xorCounts.back()++;
			} else if ( result->first == EVT_AND ) {
				andCounts.back()++;
			}
        }
		if ( result->first == eventTrigger ) {
			auto nsecs = rateBuffer.lastInterval(gpio).count();
			if ( nsecs > 0 ) { 
				emit eventInterval( rateBuffer.lastInterval(gpio).count() );
			}
			emit sampleAdc0Event();
		}
    }
}

void Daemon::sendGpioPinEvent(unsigned int gpio, EventTime event_time) {
    // reverse lookup of gpio function from given pin (first occurence)
    auto result=std::find_if(GPIO_PINMAP.begin(), GPIO_PINMAP.end(), [&gpio](const std::pair<GPIO_SIGNAL,unsigned int>& item)
                    { return item.second==gpio; }
                    );
    if (result!=GPIO_PINMAP.end()) {
		TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GPIO_EVENT);
		*(tcpMessage.dStream) << (GPIO_SIGNAL)result->first;
		emit sendTcpMessage(tcpMessage);
	}
}

void Daemon::sendBiasVoltage(){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_BIAS_VOLTAGE);
    *(tcpMessage.dStream) << biasVoltage;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendBiasStatus(){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_BIAS_SWITCH);
    *(tcpMessage.dStream) << biasON;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendGainSwitchStatus(){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GAIN_SWITCH);
    *(tcpMessage.dStream) << gainSwitch;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendPreampStatus(uint8_t channel) {
    if (channel > 1){ return; }
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_PREAMP_SWITCH);
    *(tcpMessage.dStream) << (quint8)channel << preampStatus [channel];
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendPolarityStatus() {
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_POLARITY_SWITCH);
    *(tcpMessage.dStream) << polarity1 << polarity2;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendPcaChannel(){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_PCA_SWITCH);
    *(tcpMessage.dStream) << (quint8)pcaPortMask;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendEventTriggerSelection(){
    if (pigHandler==nullptr) return;
	TcpMessage tcpMessage(TCP_MSG_KEY::MSG_EVENTTRIGGER);
	*(tcpMessage.dStream) << eventTrigger;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sendMqttStatus(bool connected){
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_MQTT_STATUS);
    *(tcpMessage.dStream) << connected;
    if (connected != mqttConnectionStatus) {
        if (connected) {
            qInfo()<<"MQTT (re)connected";
            emit GpioSetState(GPIO_PINMAP[STATUS1],1);
        }
        else {
            qWarning()<<"MQTT connection lost";
            emit GpioSetState(GPIO_PINMAP[STATUS1],0);
        }
    }
        mqttConnectionStatus=connected;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::rateCounterIntervalActualisation(){
    if (xorCounts.isEmpty()){
        xorCounts.push_back(0);
    }
    if (andCounts.isEmpty()){
        andCounts.push_back(0);
    }
    timespec now;
    timespec_get(&now, TIME_UTC);
    int64_t diff = msecdiff(now,lastRateInterval);
    while(diff>1000){
        xorCounts.push_back(0);
        andCounts.push_back(0);
        while ((quint32)xorCounts.size()>(rateBufferTime)){
            xorCounts.pop_front();
        }
        while ((quint32)andCounts.size()>(rateBufferTime)){
            andCounts.pop_front();
        }
        lastRateInterval.tv_sec += 1;
        diff = msecdiff(now,lastRateInterval);
    }
}

void Daemon::clearRates(){
    xorCounts.clear();
    xorCounts.push_back(0);
    andCounts.clear();
    andCounts.push_back(0);
    xorRatePoints.clear();
    andRatePoints.clear();
}

qreal Daemon::getRateFromCounts(quint8 which_rate){
    rateCounterIntervalActualisation();
    QList<quint64> *counts;
    if (which_rate==XOR_RATE){
        counts = &xorCounts;
    }else if( which_rate == AND_RATE){
        counts = &andCounts;
    }else{
        return -1.0;
    }
    quint64 sum = 0;
    for (auto count : *counts){
        sum += count;
    }
    timespec now;
    timespec_get(&now, TIME_UTC);
    qreal timeInterval = ((qreal)(1000*(counts->size()-1)))+(qreal)msecdiff(now,lastRateInterval); // in ms
    qreal rate = sum/timeInterval*1000;
    return (rate);
}

void Daemon::onRateBufferReminder(){
    qreal secsSinceStart = 0.001*(qreal)msecdiff(lastRateInterval,startOfProgram);
    qreal xorRate = getRateFromCounts(XOR_RATE);
    qreal andRate = getRateFromCounts(AND_RATE);
    QPointF xorPoint(secsSinceStart, xorRate);
    QPointF andPoint(secsSinceStart, andRate);
    xorRatePoints.append(xorPoint);
    andRatePoints.append(andPoint);
    emit logParameter(LogParameter("rateXOR", QString::number(xorRate)+" Hz", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("rateAND", QString::number(andRate)+" Hz", LogParameter::LOG_AVERAGE));
    while ((quint32)xorRatePoints.size()>rateMaxShowInterval/rateBufferInterval){
        xorRatePoints.pop_front();
    }
    while ((quint32)andRatePoints.size()>rateMaxShowInterval/rateBufferInterval){
        andRatePoints.pop_front();
    }
}

void Daemon::sendGpioRates(int number, quint8 whichRate){
    if (pigHandler==nullptr){
        return;
    }
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GPIO_RATE);
    QVector<QPointF> *ratePoints;
    if (whichRate==XOR_RATE){
         ratePoints = &xorRatePoints;
    }else if(whichRate==AND_RATE){
        ratePoints = &andRatePoints;
    }else{
        return;
    }
    QVector<QPointF> someRates;
    if (number >= ratePoints->size() || number == 0){
        number = ratePoints->size()-1;
    }
    if (!ratePoints->isEmpty()){
        for (int i = 0; i<number; i++){
            someRates.push_front(ratePoints->at(ratePoints->size()-1-i));
        }
    }
    *(tcpMessage.dStream) << whichRate << someRates;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::sampleAdc0Event(){
    const uint8_t channel=0;
    if (adc==nullptr || adcSamplingMode==ADC_MODE_DISABLED){
        return;
    }
    if (adc->getStatus() & i2cDevice::MODE_UNREACHABLE) return;
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_ADC_SAMPLE);
    float value = adc->readVoltage(channel);
    *(tcpMessage.dStream) << (quint8)channel << value;
    emit sendTcpMessage(tcpMessage);
    QString name="pulseHeight";
    histoMap[name].fill(value);
    emit logParameter(LogParameter("adcSamplingTime", QString::number(adc->getLastConvTime())+" ms", LogParameter::LOG_AVERAGE));
    name="adcSampleTime";
    checkRescaleHisto(histoMap[name], adc->getLastConvTime());
    histoMap[name].fill(adc->getLastConvTime());
    currentAdcSampleIndex=0;
}

void Daemon::sampleAdc0TraceEvent(){
    const uint8_t channel=0;
    if (adc==nullptr || adcSamplingMode==ADC_MODE_DISABLED){
        return;
    }
    if (adc->getStatus() & i2cDevice::MODE_UNREACHABLE) return;
    float value = adc->readVoltage(channel);
    adcSamplesBuffer.push_back(value);
    if (adcSamplesBuffer.size() > MuonPi::Config::Hardware::ADC::buffer_size/*ADC_SAMPLEBUFFER_SIZE*/) adcSamplesBuffer.pop_front();
    if (currentAdcSampleIndex>=0) {
        currentAdcSampleIndex++;
        if (currentAdcSampleIndex >= (MuonPi::Config::Hardware::ADC::buffer_size - MuonPi::Config::Hardware::ADC::pretrigger)/*ADC_SAMPLEBUFFER_SIZE-ADC_PRETRIGGER*/) {
            TcpMessage tcpMessage(TCP_MSG_KEY::MSG_ADC_TRACE);
            *(tcpMessage.dStream) << (quint16) adcSamplesBuffer.size();
            for (int i=0; i<adcSamplesBuffer.size(); i++)
                *(tcpMessage.dStream) << adcSamplesBuffer[i];
            emit sendTcpMessage(tcpMessage);
            currentAdcSampleIndex = -1;
            //qDebug()<<"adc trace sent";
        }
    }
    emit logParameter(LogParameter("adcSamplingTime", QString::number(adc->getLastConvTime())+" ms", LogParameter::LOG_AVERAGE));
    checkRescaleHisto(histoMap["adcSampleTime"], adc->getLastConvTime());
    histoMap["adcSampleTime"].fill(adc->getLastConvTime());
}

void Daemon::sampleAdcEvent(uint8_t channel){
    if (adc==nullptr || adcSamplingMode==ADC_MODE_DISABLED){
        return;
    }
    if (adc->getStatus() & i2cDevice::MODE_UNREACHABLE) return;
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_ADC_SAMPLE);
    float value = adc->readVoltage(channel);
    *(tcpMessage.dStream) << (quint8)channel << value;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::getTemperature(){
    if (lm75==nullptr){
        return;
    }
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_TEMPERATURE);
    if (lm75->getStatus() & i2cDevice::MODE_UNREACHABLE) return;
    float value = lm75->getTemperature();
    *(tcpMessage.dStream) << value;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::setEventTriggerSelection(GPIO_SIGNAL signal) {
    if (pigHandler==nullptr) return;
    auto it=GPIO_PINMAP.find(signal);
    if (it==GPIO_PINMAP.end()) {
		return;
	}

    if ( verbose > 0 ){
        qInfo() << "changed event selection to signal" << GPIO_SIGNAL_MAP[signal].name;
    }
	emit GpioRegisterForCallback( it->second, PigpiodHandler::EventEdge::RisingEdge );
	eventTrigger = signal;
    emit logParameter(LogParameter("gpioTriggerSelection", "0x"+QString::number((int)signal,16), LogParameter::LOG_EVERY));
}

// ALL FUNCTIONS ABOUT SETTINGS FOR THE I2C-DEVICES (DAC, ADC, PCA...)
void Daemon::setPcaChannel(uint8_t channel) {
    if (!pca || !pca->devicePresent()) {
        return;
    }
    if (channel > ((HW_VERSION==1)?3:7)) {
        qWarning() << "invalid PCA channel selection: ch" <<(int)channel<<"...ignoring";
        return;
    }
    if (verbose > 0){
        qInfo() << "changed pcaPortMask to " << channel;
    }
    pcaPortMask = channel;
    pca->setOutputState(channel);
    emit logParameter(LogParameter("ubxInputSwitch", "0x"+QString::number(pcaPortMask,16), LogParameter::LOG_EVERY));
    //sendPcaChannel();
}

void Daemon::setBiasVoltage(float voltage) {
    biasVoltage = voltage;
    if (verbose > 0){
        qInfo() << "change biasVoltage to " << voltage;
    }
    if (dac && dac->devicePresent()) {
        dac->setVoltage(DAC_BIAS, voltage);
        emit logParameter(LogParameter("biasDAC", QString::number(voltage)+" V", LogParameter::LOG_EVERY));
    }
    clearRates();
    //sendBiasVoltage();
}

void Daemon::setBiasStatus(bool status){
    biasON = status;
    if (verbose > 0){
        qInfo() << "change biasStatus to " << status;
    }
    //emit GpioSetState(UBIAS_EN, status);

    if (status) {
        emit GpioSetState(GPIO_PINMAP[UBIAS_EN], (HW_VERSION==1)?1:0);
    }
    else {
        emit GpioSetState(GPIO_PINMAP[UBIAS_EN], (HW_VERSION==1)?0:1);
    }
    emit logParameter(LogParameter("biasSwitch", QString::number(status), LogParameter::LOG_EVERY));
    clearRates();
    //sendBiasStatus();
}

void Daemon::setDacThresh(uint8_t channel, float threshold) {
    if (threshold < 0 || channel > 3) { return; }
    if (channel==2 || channel==3) {
        if (dac->devicePresent()) {
            dac->setVoltage(channel, threshold);
        }
        return;
    }
    if (threshold > 4.095){
        threshold = 4.095;
    }
    if (verbose > 0){
        qInfo() << "change dacThresh " << channel << " to " << threshold;
    }
    dacThresh[channel] = threshold;
    clearRates();
    if (dac->devicePresent()) {
        dac->setVoltage(channel, threshold);
        emit logParameter(LogParameter("thresh"+QString::number(channel+1), QString::number(dacThresh[channel])+" V", LogParameter::LOG_EVERY));
    }
    //sendDacThresh(channel);
}

void Daemon::saveDacValuesToEeprom(){
    for (int i=0; i<4; i++) {
        MCP4728::DacChannel dacChannel;
        dac->readChannel(i, dacChannel);
        dacChannel.eeprom=true;
        dac->writeChannel(i, dacChannel);
    }
}

bool Daemon::readEeprom()
{
    if (eep==nullptr) return false;
    if (eep->devicePresent()) {
        if (verbose>2) cout<<"eep device is present."<<endl;
    } else {
        cerr<<"eeprom device NOT present!"<<endl;
        return false;
    }
    uint16_t n=256;
    uint8_t buf[256];
    for (int i=0; i<n; i++) buf[i]=0;
    bool retval=(eep->readBytes(0,n,buf)==n);
    cout<<"*** EEPROM content ***"<<endl;
    for (int j=0; j<16; j++) {
        cout<<hex<<std::setfill ('0') << std::setw (2)<<j*16<<": ";
        for (int i=0; i<16; i++) {
            cout<<hex<<std::setfill ('0') << std::setw (2)<<(int)buf[j*16+i]<<" ";
        }
        cout<<endl;
    }
    return retval;
}

void Daemon::setUbxMsgRates(QMap<uint16_t, int>& ubxMsgRates){
    for (QMap<uint16_t, int>::iterator it = ubxMsgRates.begin(); it != ubxMsgRates.end(); it++) {
        emit UBXSetCfgMsgRate(it.key(),1,it.value());
        emit sendPollUbxMsgRate(it.key());
        waitingForAppliedMsgRate++;
    }
}

// ALL FUNCTIONS ABOUT UBLOX GPS MODULE
void Daemon::configGps() {
    // set up ubx as only outPortProtocol
    //emit UBXSetCfgPrt(1,1); // enables on UART port (1) only the UBX protocol
    emit UBXSetCfgPrt(1, PROTO_UBX);

    // set dynamic model: Stationary
    emit UBXSetDynModel(2);

    emit UBXSetAopCfg(true);

    emit sendPollUbxMsg(UBX_MON_VER);

    // deactivate all NMEA messages: (port 6 means ALL ports)
    // not needed because of deactivation of all NMEA messages with "UBXSetCfgPrt"
//    msgRateCfgs.insert(UBX_NMEA_DTM,0);
//    // msgRateCfgs.insert(UBX_NMEA_GBQ,0);
//    msgRateCfgs.insert(UBX_NMEA_GBS,0);
//    msgRateCfgs.insert(UBX_NMEA_GGA,0);
//    msgRateCfgs.insert(UBX_NMEA_GLL,0);
//    // msgRateCfgs.insert(UBX_NMEA_GLQ,0);
//    // msgRateCfgs.insert(UBX_NMEA_GNQ,0);
//    msgRateCfgs.insert(UBX_NMEA_GNS,0);
//    // msgRateCfgs.insert(UBX_NMEA_GPQ,0);
//    msgRateCfgs.insert(UBX_NMEA_GRS,0);
//    msgRateCfgs.insert(UBX_NMEA_GSA,0);
//    msgRateCfgs.insert(UBX_NMEA_GST,0);
//    msgRateCfgs.insert(UBX_NMEA_GSV,0);
//    msgRateCfgs.insert(UBX_NMEA_RMC,0);
//    // msgRateCfgs.insert(UBX_NMEA_TXT,0);
//    msgRateCfgs.insert(UBX_NMEA_VLW,0);
//    msgRateCfgs.insert(UBX_NMEA_VTG,0);
//    msgRateCfgs.insert(UBX_NMEA_ZDA,0);
//    msgRateCfgs.insert(UBX_NMEA_POSITION,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_DTM,6,0);
//    // has no output msg UBX_NMEA_GBQ
//    emit UBXSetCfgMsgRate(UBX_NMEA_GBS,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_GGA,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_GLL,6,0);
//    // has no output msg UBX_NMEA_GLQ
//    // has no output msg UBX_NMEA_GNQ
//    emit UBXSetCfgMsgRate(UBX_NMEA_GNS,6,0);
//    // has no output msg UBX_NMEA_GPQ
//    emit UBXSetCfgMsgRate(UBX_NMEA_GRS,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_GSA,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_GST,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_GSV,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_RMC,6,0);
//    // is not configured through UBX_CFG_MSG but through UBX-CFG-INF!!!  (UBX_NMEA_TXT)
//    //emit UBXSetCfgMsgRate(UBX_NMEA_VLW,6,0); don't know why this does not work, probably not supported anymore
//    emit UBXSetCfgMsgRate(UBX_NMEA_VTG,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_ZDA,6,0);
//    emit UBXSetCfgMsgRate(UBX_NMEA_POSITION,6,0);

    // set protocol configuration for ports
    // msgRateCfgs: -1 means unknown, 0 means off, some positive value means update time
    int measrate = 10;
    // msgRateCfgs.insert(UBX_CFG_RATE, measrate);
    // msgRateCfgs.insert(UBX_TIM_TM2, 1);
    // msgRateCfgs.insert(UBX_TIM_TP, 51);
    // msgRateCfgs.insert(UBX_NAV_TIMEUTC, 20);
    // msgRateCfgs.insert(UBX_MON_HW, 47);
    // msgRateCfgs.insert(UBX_NAV_SAT, 59);
    // msgRateCfgs.insert(UBX_NAV_TIMEGPS, 61);
    // msgRateCfgs.insert(UBX_NAV_SOL, 67);
    // msgRateCfgs.insert(UBX_NAV_STATUS, 71);
    // msgRateCfgs.insert(UBX_NAV_CLOCK, 89);
    // msgRateCfgs.insert(UBX_MON_TXBUF, 97);
    // msgRateCfgs.insert(UBX_NAV_SBAS, 255);
    // msgRateCfgs.insert(UBX_NAV_DOP, 101);
    // msgRateCfgs.insert(UBX_NAV_SVINFO, 49);
    emit UBXSetCfgRate(1000/measrate, 1); // UBX_RATE

    //emit sendPollUbxMsg(UBX_MON_VER);
    emit UBXSetCfgMsgRate(UBX_TIM_TM2, 1, 1);	// TIM-TM2
    emit UBXSetCfgMsgRate(UBX_TIM_TP, 1, 0);	// TIM-TP
    emit UBXSetCfgMsgRate(UBX_NAV_TIMEUTC, 1, 131);	// NAV-TIMEUTC
    emit UBXSetCfgMsgRate(UBX_MON_HW, 1, 47);	// MON-HW
    emit UBXSetCfgMsgRate(UBX_MON_HW2, 1, 49);	// MON-HW
    emit UBXSetCfgMsgRate(UBX_NAV_POSLLH, 1, 127);	// MON-POSLLH
    // probably also configured with UBX-CFG-INFO...
    emit UBXSetCfgMsgRate(UBX_NAV_TIMEGPS, 1, 0);	// NAV-TIMEGPS
    emit UBXSetCfgMsgRate(UBX_NAV_SOL, 1, 0);	// NAV-SOL
    emit UBXSetCfgMsgRate(UBX_NAV_STATUS, 1, 71);	// NAV-STATUS
    emit UBXSetCfgMsgRate(UBX_NAV_CLOCK, 1, 189);	// NAV-CLOCK
    emit UBXSetCfgMsgRate(UBX_MON_RXBUF, 1, 53);	// MON-TXBUF
    emit UBXSetCfgMsgRate(UBX_MON_TXBUF, 1, 51);	// MON-TXBUF
    emit UBXSetCfgMsgRate(UBX_NAV_SBAS, 1, 0);	// NAV-SBAS
    emit UBXSetCfgMsgRate(UBX_NAV_DOP, 1, 254);	// NAV-DOP
    // this poll is for checking the port cfg (which protocols are enabled etc.)
    emit sendPollUbxMsg(UBX_CFG_PRT);
    emit sendPollUbxMsg(UBX_MON_VER);
    emit sendPollUbxMsg(UBX_MON_VER);
    emit sendPollUbxMsg(UBX_MON_VER);
    emit sendPollUbxMsg(UBX_CFG_GNSS);
    //emit UBXSetMinCNO(5);
    emit sendPollUbxMsg(UBX_CFG_NAVX5);
    emit sendPollUbxMsg(UBX_CFG_ANT);
    emit sendPollUbxMsg(UBX_CFG_TP5);

    configGpsForVersion();
}

void Daemon::configGpsForVersion() {
    if (QtSerialUblox::getProtVersion()<=0.1) return;
    if (QtSerialUblox::getProtVersion()>15.0) {
        if (std::find(allMsgCfgID.begin(), allMsgCfgID.end(), UBX_NAV_SAT)==allMsgCfgID.end()) {
            allMsgCfgID.push_back(UBX_NAV_SAT);
        }
        emit UBXSetCfgMsgRate(UBX_NAV_SAT, 1, 69);	// NAV-SAT
        emit UBXSetCfgMsgRate(UBX_NAV_SVINFO, 1, 0);
    } else emit UBXSetCfgMsgRate(UBX_NAV_SVINFO, 1, 69);	// NAV-SVINFO
    //cout<<"prot Version: "<<QtSerialUblox::getProtVersion()<<endl;
}

void Daemon::pollAllUbxMsgRate() {
    for (const auto& elem : allMsgCfgID) {
        emit sendPollUbxMsgRate(elem);
    }
}

void Daemon::UBXReceivedAckNak(uint16_t ackedMsgID, uint16_t ackedCfgMsgID) {
    // the value was already set correctly before by either poll or set,
    // if not acknowledged or timeout we set the value to -1 (unknown/undefined)
    switch (ackedMsgID) {
    case UBX_CFG_MSG:
        msgRateCfgs.insert(ackedCfgMsgID, -1);
        break;
    default:
        break;
    }
}

void Daemon::UBXReceivedMsgRateCfg(uint16_t msgID, uint8_t rate) {
    msgRateCfgs.insert(msgID, rate);
    waitingForAppliedMsgRate--;
    if (waitingForAppliedMsgRate<0){
        waitingForAppliedMsgRate = 0;
    }
    if (waitingForAppliedMsgRate==0){
        sendUbxMsgRates();
    }
}
/*
void Daemon::onGpsMonHWUpdated(uint16_t noise, uint16_t agc, uint8_t antStatus, uint8_t antPower, uint8_t jamInd, uint8_t flags)
{
    TcpMessage tcpMessage(gpsMonHWSig);
    (*tcpMessage.dStream) << (quint16)noise << (quint16)agc << (quint8) antStatus
    << (quint8)antPower << (quint8)jamInd << (quint8)flags;
    emit sendTcpMessage(tcpMessage);
    emit logParameter(LogParameter("preampNoise", QString::number(-noise)+" dBHz", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("preampAGC", QString::number(agc), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("antennaStatus", QString::number(antStatus), LogParameter::LOG_LATEST));
    emit logParameter(LogParameter("antennaPower", QString::number(antPower), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("jammingLevel", QString::number(jamInd/2.55,'f',1)+" %", LogParameter::LOG_AVERAGE));
}

void Daemon::onGpsMonHW2Updated(int8_t ofsI, uint8_t magI, int8_t ofsQ, uint8_t magQ, uint8_t cfgSrc)
{
    TcpMessage tcpMessage(gpsMonHW2Sig);
    (*tcpMessage.dStream) << (qint8)ofsI << (quint8)magI << (qint8)ofsQ
    << (quint8)magQ << (quint8)cfgSrc;
    emit sendTcpMessage(tcpMessage);
}
*/
void Daemon::onGpsMonHWUpdated(const GnssMonHwStruct& hw)
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_MONHW);
    (*tcpMessage.dStream) << hw;
    emit sendTcpMessage(tcpMessage);
    emit logParameter(LogParameter("preampNoise", QString::number(-hw.noise)+" dBHz", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("preampAGC", QString::number(hw.agc), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("antennaStatus", QString::number(hw.antStatus), LogParameter::LOG_LATEST));
    emit logParameter(LogParameter("antennaPower", QString::number(hw.antPower), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("jammingLevel", QString::number(hw.jamInd/2.55,'f',1)+" %", LogParameter::LOG_AVERAGE));
}

void Daemon::onGpsMonHW2Updated(const GnssMonHw2Struct& hw2)
{
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_MONHW2);
    (*tcpMessage.dStream) << hw2;
    emit sendTcpMessage(tcpMessage);
}

void Daemon::onGpsPropertyUpdatedGnss(const std::vector<GnssSatellite>& sats,
    std::chrono::duration<double> lastUpdated) {
    vector<GnssSatellite> visibleSats = sats;
    std::sort(visibleSats.begin(), visibleSats.end(), GnssSatellite::sortByCnr);
    while (visibleSats.size() > 0 && visibleSats.back().getCnr() == 0) visibleSats.pop_back();

    if (verbose > 3) {
        cout << std::chrono::system_clock::now()
            - std::chrono::duration_cast<std::chrono::microseconds>(lastUpdated)
            << "Nr of satellites: " << visibleSats.size() <<" (out of "<< sats.size() << endl;
        // read nrSats property without evaluation to prevent separate display of this property
        // in the common message poll below
        GnssSatellite::PrintHeader(true);
        for (unsigned int i = 0; i < sats.size(); i++) {
            sats[i].Print(i, false);
        }
    }
    int N=sats.size();
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_GNSS_SATS);
    (*tcpMessage.dStream) << N;
    for (int i=0; i<N; i++) {
        (*tcpMessage.dStream)<< sats [i];
    }
    emit sendTcpMessage(tcpMessage);
    //nrSats=QVariant(N);
    nrSats=Property("nrSats",N);
    nrVisibleSats = QVariant { static_cast<qulonglong>(visibleSats.size()) };

    propertyMap["nrSats"]=Property("nrSats",N);
    propertyMap["visSats"]=Property("visSats",visibleSats.size());

    int usedSats=0, maxCnr=0;
    if (visibleSats.size()) {
        for (auto sat : visibleSats){
            if (sat.fUsed) usedSats++;
            if (sat.fCnr>maxCnr) maxCnr=sat.fCnr;
        }
    }
    propertyMap["usedSats"]=Property("usedSats",usedSats);
    propertyMap["maxCNR"]=Property("maxCNR",maxCnr);
    //qDebug()<< "propertyMap.size()="<<propertyMap.size();

    //qDebug() << "received satlist ok, size=" << satlist.size();
    emit logParameter(LogParameter("sats",QString("%1").arg(visibleSats.size()), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("usedSats",QString("%1").arg(usedSats), LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("maxCNR",QString("%1").arg(maxCnr)+" dB", LogParameter::LOG_AVERAGE));
}

void Daemon::onUBXReceivedGnssConfig(uint8_t numTrkCh, const std::vector<GnssConfigStruct>& gnssConfigs) {
    if (verbose > 3) {
        // put some verbose output here
    }
    int N=gnssConfigs.size();
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_GNSS_CONFIG);
    (*tcpMessage.dStream) << (int)numTrkCh << N;
    for (int i=0; i<N; i++) {
        (*tcpMessage.dStream)<<gnssConfigs[i].gnssId<<gnssConfigs[i].resTrkCh<<
        gnssConfigs[i].maxTrkCh<<gnssConfigs[i].flags;
    }
    emit sendTcpMessage(tcpMessage);
}

void Daemon::onUBXReceivedTP5(const UbxTimePulseStruct& tp) {
    static uint8_t forceUtcSetCounter = 0;
    if (verbose > 3) {
        // put some verbose output here
    }
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_CFG_TP5);
    (*tcpMessage.dStream) << tp;
    emit sendTcpMessage(tcpMessage);
    // check here if UTC is selected as time source
    // this should probably be implemented somewhere else, maybe at ublox init
    // however, for the timestamping to work correctly, setting the time grid to UTC is mandatory!
    int timeGrid = (tp.flags & UbxTimePulseStruct::GRID_UTC_GPS)>>7;
    if (timeGrid != 0 && forceUtcSetCounter++ < 3) {
        UbxTimePulseStruct newTp = tp;
        newTp.flags &= ~((uint32_t)UbxTimePulseStruct::GRID_UTC_GPS);
        emit UBXSetCfgTP5(newTp);
        qDebug() << "forced time grid to UTC";
        emit sendPollUbxMsg(UBX_CFG_TP5);
    }
}

void Daemon::onUBXReceivedTxBuf(uint8_t txUsage, uint8_t txPeakUsage) {
    TcpMessage* tcpMessage;
    if (verbose>3) {
        qDebug() << "TX buf usage: " << (int)txUsage << " %";
        qDebug() << "TX buf peak usage: " << (int)txPeakUsage << " %";
    }
    tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_TXBUF);
    *(tcpMessage->dStream) << (quint8)txUsage << (quint8)txPeakUsage;
    emit sendTcpMessage(*tcpMessage);
    delete tcpMessage;
    emit logParameter(LogParameter("TXBufUsage", QString::number(txUsage)+" %", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("maxTXBufUsage", QString::number(txPeakUsage)+" %", LogParameter::LOG_LATEST));
}

void Daemon::onUBXReceivedRxBuf(uint8_t rxUsage, uint8_t rxPeakUsage) {
    TcpMessage* tcpMessage;
    if (verbose>3) {
        qDebug() << "RX buf usage: " << (int)rxUsage << " %";
        qDebug() << "RX buf peak usage: " << (int)rxPeakUsage << " %";
    }
    tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_RXBUF);
    *(tcpMessage->dStream) << (quint8)rxUsage << (quint8)rxPeakUsage;
    emit sendTcpMessage(*tcpMessage);
    delete tcpMessage;
    emit logParameter(LogParameter("RXBufUsage", QString::number(rxUsage)+" %", LogParameter::LOG_AVERAGE));
    emit logParameter(LogParameter("maxRXBufUsage", QString::number(rxPeakUsage)+" %", LogParameter::LOG_LATEST));
}

void Daemon::gpsPropertyUpdatedUint8(uint8_t data, std::chrono::duration<double> updateAge,
    char propertyName) {
    TcpMessage* tcpMessage;
    switch (propertyName) {
    case 's':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "Nr of available satellites: " << (int)data << endl;
        break;
    case 'e':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "quant error: " << (int)data << " ps" << endl;
        break;
    /*
    case 'b':
        if (verbose>2)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "TX buf usage: " << (int)data << " %" << endl;
        tcpMessage = new TcpMessage(gpsTxBufSig);
        *(tcpMessage->dStream) << (quint8)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("TXBufUsage", QString::number(data)+" %", LogParameter::LOG_LATEST));
        break;
    case 'p':
        if (verbose>2)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "TX buf peak usage: " << (int)data << " %" << endl;
        tcpMessage = new TcpMessage(gpsTxBufPeakSig);
        *(tcpMessage->dStream) << (quint8)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("maxTXBufUsage", QString::number(data)+" %", LogParameter::LOG_LATEST));
        break;
    */
    case 'f':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "Fix value: " << (int)data << endl;
        tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_FIXSTATUS);
        *(tcpMessage->dStream) << (quint8)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("fixStatus", QString::number(data), LogParameter::LOG_LATEST));
        emit logParameter(LogParameter("fixStatusString", FIX_TYPE_STRINGS[data], LogParameter::LOG_LATEST));
        fixStatus=QVariant(data);
        propertyMap["fixStatus"]=Property("fixStatus",FIX_TYPE_STRINGS[data]);
        break;
    default:
        break;
    }
}

void Daemon::gpsPropertyUpdatedUint32(uint32_t data, chrono::duration<double> updateAge,
    char propertyName) {
    TcpMessage* tcpMessage;
    switch (propertyName) {
    case 'a':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "time accuracy: " << data << " ns" << endl;
        tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_TIME_ACCURACY);
        *(tcpMessage->dStream) << (quint32)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("timeAccuracy", QString::number(data)+" ns", LogParameter::LOG_AVERAGE));
        break;
    case 'f':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "frequency accuracy: " << data << " ps/s" << endl;
        tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_FREQ_ACCURACY);
        *(tcpMessage->dStream) << (quint32)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("freqAccuracy", QString::number(data)+" ps/s", LogParameter::LOG_AVERAGE));
        break;
    case 'u':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "Ublox uptime: " << data << " s" << endl;
        tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_UPTIME);
        *(tcpMessage->dStream) << (quint32)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        emit logParameter(LogParameter("ubloxUptime", QString::number(data)+" s", LogParameter::LOG_LATEST));
        break;
    case 'c':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "rising edge counter: " << data << endl;
        propertyMap["events"]=Property("events",(quint16)data);
        tcpMessage = new TcpMessage(TCP_MSG_KEY::MSG_UBX_EVENTCOUNTER);
        *(tcpMessage->dStream) << (quint32)data;
        emit sendTcpMessage(*tcpMessage);
        delete tcpMessage;
        break;
    default:
        break;
    }
}

void Daemon::gpsPropertyUpdatedInt32(int32_t data, std::chrono::duration<double> updateAge,
    char propertyName) {
    switch (propertyName) {
    case 'd':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "clock drift: " << data << " ns/s" << endl;
        logParameter(LogParameter("clockDrift", QString::number(data)+" ns/s", LogParameter::LOG_AVERAGE));
        propertyMap["clkDrift"]=Property("clkDrift",(qint32)data);
        break;
    case 'b':
        if (verbose>3)
            cout << std::chrono::system_clock::now()
                - std::chrono::duration_cast<std::chrono::microseconds>(updateAge)
                << "clock bias: " << data << " ns" << endl;
        emit logParameter(LogParameter("clockBias", QString::number(data)+" ns", LogParameter::LOG_AVERAGE));
        propertyMap["clkBias"]=Property("clkBias",(qint32)data);
        break;
    default:
        break;
    }
}


void Daemon::UBXReceivedVersion(const QString& swString, const QString& hwString, const QString& protString)
{
    static bool initialVersionInfo = true;
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_VERSION);
    (*tcpMessage.dStream) << swString << hwString << protString;
    emit sendTcpMessage(tcpMessage);
    logParameter(LogParameter("UBX_SW_Version", swString, LogParameter::LOG_ONCE));
    logParameter(LogParameter("UBX_HW_Version", hwString, LogParameter::LOG_ONCE));
    logParameter(LogParameter("UBX_Prot_Version", protString, LogParameter::LOG_ONCE));
    if (initialVersionInfo) {
        configGpsForVersion();
        qInfo()<<"Ublox version: "<<hwString<<" (fw:"<<swString<<"prot:"<<protString<<")";
    }
    initialVersionInfo = false;
}

void Daemon::toConsole(const QString &data) {
    cout << data << endl;
}

void Daemon::gpsToConsole(const QString &data) {
    cout << data << flush;
}

void Daemon::gpsConnectionError() {

}


// ALL OTHER UTITLITY FUNCTIONS
void Daemon::onMadeConnection(QString remotePeerAddress, quint16 remotePeerPort, QString /*localAddress*/, quint16 /*localPort*/) {
    if (verbose>3) cout << "established connection with " << remotePeerAddress << ":" << remotePeerPort << endl;

    emit sendPollUbxMsg(UBX_MON_VER);
    emit sendPollUbxMsg(UBX_CFG_GNSS);
    emit sendPollUbxMsg(UBX_CFG_NAV5);
    emit sendPollUbxMsg(UBX_CFG_TP5);
    emit sendPollUbxMsg(UBX_CFG_NAVX5);

    sendBiasStatus();
    sendBiasVoltage();
    sendDacThresh(0);
    sendDacThresh(1);
    sendPcaChannel();
    sendEventTriggerSelection();
}

void Daemon::onStoppedConnection(QString remotePeerAddress, quint16 remotePeerPort, QString /*localAddress*/, quint16 /*localPort*/,
        quint32 timeoutTime, quint32 connectionDuration) {
    if (verbose>3) {
        qDebug() << "stopped connection with " << remotePeerAddress << ":" << remotePeerPort << endl;
        qDebug() << "connection timeout at " << timeoutTime << "  connection lasted " << connectionDuration << "s" << endl;
    }
}

void Daemon::displayError(QString message)
{
    qDebug() << "Daemon: " << message;
}

void Daemon::displaySocketError(int socketError, QString message)
{
    switch (socketError) {
    case QAbstractSocket::HostNotFoundError:
        qCritical() << tr("The host was not found. Please check the host and port settings.");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qCritical() << tr("The connection was refused by the peer. "
            "Make sure the server is running, "
            "and check that the host name and port "
            "settings are correct.");
        break;
    default:
        qCritical() << tr("The following error occurred: %1.").arg(message);
    }
    flush(cout);
}

void Daemon::delay(int millisecondsWait)
{
    QEventLoop loop;
    QTimer t;
    t.connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(millisecondsWait);
    loop.exec();
}

void Daemon::printTimestamp()
{
    std::chrono::time_point<std::chrono::system_clock> timestamp = std::chrono::system_clock::now();
    std::chrono::microseconds mus = std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch());
    std::chrono::seconds secs = std::chrono::duration_cast<std::chrono::seconds>(mus);
    std::chrono::microseconds subs = mus - secs;


    // 	t1 = std::chrono::system_clock::now();
    // 	t2 = std::chrono::duration_cast<std::chrono::seconds>(t1);
        //double subs=timestamp-(long int)timestamp;
    cout << secs.count() << "." << setw(6) << setfill('0') << subs.count() << " " << setfill(' ');
}

// some signal handling stuff
void Daemon::hupSignalHandler(int)
{
    char a = 1;
    ::write(sighupFd[0], &a, sizeof(a));
}

void Daemon::termSignalHandler(int)
{
    char a = 1;
    ::write(sigtermFd[0], &a, sizeof(a));
}
void Daemon::intSignalHandler(int) {
    char a = 1;
    ::write(sigintFd[0], &a, sizeof(a));
}

void Daemon::aquireMonitoringParameters()
{
    if (lm75 && lm75->devicePresent()) emit logParameter(LogParameter("temperature", QString::number(lm75->getTemperature())+" degC", LogParameter::LOG_AVERAGE));

    double v1=0.,v2=0.;
    if (adc && (!(adc->getStatus() & i2cDevice::MODE_UNREACHABLE)) && (adc->getStatus() & (i2cDevice::MODE_NORMAL | i2cDevice::MODE_FORCE))) {
        v1=adc->readVoltage(2);
        v2=adc->readVoltage(3);
        if (calib && calib->getCalibItem("VDIV").name=="VDIV") {
            CalibStruct vdivItem=calib->getCalibItem("VDIV");
            std::istringstream istr(vdivItem.value);
            double vdiv;
            istr >> vdiv;
            vdiv/=100.;
            logParameter(LogParameter("calib_vdiv", QString::number(vdiv), LogParameter::LOG_ONCE));
//			istr=std::istringstream(calib->getCalibItem("RSENSE").value);
            istr.clear();
            istr.str(calib->getCalibItem("RSENSE").value);
            double rsense;
            istr >> rsense;
            if (verbose>2){
                qDebug()<<"rsense:"<<QString::fromStdString(calib->getCalibItem("RSENSE").value)<<" ("<<rsense<<")";
            }
            rsense /= 10.*1000.; // yields Rsense in MOhm
            logParameter(LogParameter("calib_rsense", QString::number(rsense*1000.)+" kOhm", LogParameter::LOG_ONCE));
//			logParameter(LogParameter("vbias1", QString::number(v1*vdiv)+" V", LogParameter::LOG_AVERAGE));
//			logParameter(LogParameter("vbias2", QString::number(v2*vdiv)+" V", LogParameter::LOG_AVERAGE));
            double ubias=v2*vdiv;
            logParameter(LogParameter("vbias", QString::number(ubias)+" V", LogParameter::LOG_AVERAGE));
            checkRescaleHisto(histoMap["Bias Voltage"], ubias);
            histoMap["Bias Voltage"].fill(ubias);
            double usense=(v1-v2)*vdiv;
            logParameter(LogParameter("vsense", QString::number(usense)+" V", LogParameter::LOG_AVERAGE));

            CalibStruct flagItem=calib->getCalibItem("CALIB_FLAGS");
            int calFlags=0;
//			istr=std::istringstream(flagItem.value);

            istr.clear();
            istr.str(flagItem.value);
            istr >> calFlags;
            if (verbose>2){
                qDebug()<<"cal flags:"<<QString::fromStdString(flagItem.value)<<" ("<<(int)calFlags<<dec<<")";
            }
            double icorr = 0.;
            if (calFlags & CalibStruct::CALIBFLAGS_CURRENT_COEFFS) {
                double islope,ioffs;
                istr.clear();
                istr.str(calib->getCalibItem("COEFF2").value);
                istr >> ioffs;
                logParameter(LogParameter("calib_coeff2", QString::number(ioffs), LogParameter::LOG_ONCE));
                istr.clear();
                istr.str(calib->getCalibItem("COEFF3").value);
                istr >> islope;
                logParameter(LogParameter("calib_coeff3", QString::number(islope), LogParameter::LOG_ONCE));
                icorr = ubias*islope+ioffs;
            }
            double ibias = usense/rsense-icorr;
            checkRescaleHisto(histoMap["Bias Current"], ibias);
            histoMap["Bias Current"].fill(ibias);
            logParameter(LogParameter("ibias", QString::number(ibias)+" uA", LogParameter::LOG_AVERAGE));

        } else {
            logParameter(LogParameter("vadc3", QString::number(v1)+" V", LogParameter::LOG_AVERAGE));
            logParameter(LogParameter("vadc4", QString::number(v2)+" V", LogParameter::LOG_AVERAGE));
        }
    }
}

void Daemon::onLogParameterPolled(){
    // connect to the regular log timer signal to log several non-regularly polled parameters
    /*
    double xorRate = 0.0;
    if (!xorRatePoints.isEmpty()){
        xorRate = xorRatePoints.back().y();
    }
    logParameter(LogParameter("rateXOR", QString::number(xorRate)+" Hz"));
    double andRate = 0.0;
    if (!andRatePoints.isEmpty()){
        andRate = andRatePoints.back().y();
    }
    logParameter(LogParameter("rateAND", QString::number(andRate)+" Hz"));
    */
    emit logParameter(LogParameter("biasSwitch", QString::number(biasON), LogParameter::LOG_ON_CHANGE));
    emit logParameter(LogParameter("preampSwitch1", QString::number((int)preampStatus[0]), LogParameter::LOG_ON_CHANGE));
    emit logParameter(LogParameter("preampSwitch2", QString::number((int)preampStatus[1]), LogParameter::LOG_ON_CHANGE));
    emit logParameter(LogParameter("gainSwitch", QString::number((int)gainSwitch), LogParameter::LOG_ON_CHANGE));
    emit logParameter(LogParameter("polaritySwitch1", QString::number((int)polarity1), LogParameter::LOG_ON_CHANGE));
    emit logParameter(LogParameter("polaritySwitch2", QString::number((int)polarity2), LogParameter::LOG_ON_CHANGE));
//    if (lm75 && lm75->devicePresent()) emit logParameter(LogParameter("temperature", QString::number(lm75->getTemperature())+" degC", LogParameter::LOG_AVERAGE));

    if (dac && dac->devicePresent()) {
        emit logParameter(LogParameter("thresh1", QString::number(dacThresh[0])+" V", LogParameter::LOG_ON_CHANGE));
        emit logParameter(LogParameter("thresh2", QString::number(dacThresh[1])+" V", LogParameter::LOG_ON_CHANGE));
        emit logParameter(LogParameter("biasDAC", QString::number(biasVoltage)+" V", LogParameter::LOG_ON_CHANGE));
    }

    if (pca && pca->devicePresent()) emit logParameter(LogParameter("ubxInputSwitch", "0x"+QString::number(pcaPortMask,16), LogParameter::LOG_ON_CHANGE));
    if (pigHandler!=nullptr) emit logParameter(LogParameter("gpioTriggerSelection", "0x"+QString::number((int)eventTrigger,16), LogParameter::LOG_ON_CHANGE));
    //logBiasValues();
    if (adc && !(adc->getStatus() & i2cDevice::MODE_UNREACHABLE)) {
/*
        emit logParameter(LogParameter("adcSamplingTime", QString::number(adc->getLastConvTime())+" ms", LogParameter::LOG_ON_CHANGE));
        checkRescaleHisto(adcSampleTimeHisto, adc->getLastConvTime());
        adcSampleTimeHisto.fill(adc->getLastConvTime());
*/
//        sendHistogram(histoMap["adcSampleTime"]);
//        sendHistogram(histoMap["pulseHeight"]);
    }
/*
    sendHistogram(ubxTimeLengthHisto);
    sendHistogram(eventIntervalHisto);
    sendHistogram(eventIntervalShortHisto);
    sendHistogram(ubxTimeIntervalHisto);
    sendHistogram(tpTimeDiffHisto);
    sendHistogram(tdc7200Histo);
*/

    for (const auto &hist : histoMap) {
        sendHistogram(hist);
    }

    sendLogInfo();
    if (verbose>2)
    {
        qDebug() << "current data file: " << fileHandler->dataFileInfo().absoluteFilePath();
        qDebug() << " file size: " << fileHandler->dataFileInfo().size()/(1024*1024) << "MiB";
    }

//      Since Linux 2.3.23 (i386) and Linux 2.3.48 (all architectures) the
//        structure is:
//
//            struct sysinfo {
//                long uptime;             /* Seconds since boot */
//                unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
//                unsigned long totalram;  /* Total usable main memory size */
//                unsigned long freeram;   /* Available memory size */
//                unsigned long sharedram; /* Amount of shared memory */
//                unsigned long bufferram; /* Memory used by buffers */
//                unsigned long totalswap; /* Total swap space size */
//                unsigned long freeswap;  /* Swap space still available */
//                unsigned short procs;    /* Number of current processes */
//                unsigned long totalhigh; /* Total high memory size */
//                unsigned long freehigh;  /* Available high memory size */
//                unsigned int mem_unit;   /* Memory unit size in bytes */
//                char _f[20-2*sizeof(long)-sizeof(int)];
//                                         /* Padding to 64 bytes */
//            };
//
//        In the above structure, sizes of the memory and swap fields are given
//        as multiples of mem_unit bytes.

    struct sysinfo info;
    memset(&info, 0, sizeof info);
    int err=sysinfo(&info);
    if (!err) {
        double f_load = 1.0 / (1 << SI_LOAD_SHIFT);
        if (verbose>2) {
            qDebug()<<"*** Sysinfo Stats ***";
            qDebug()<<"nr of cpus      : "<<get_nprocs();
            qDebug()<<"uptime (h)      : "<<info.uptime/3600.;
            qDebug()<<"load avg (1min) : "<<info.loads[0]*f_load;
            qDebug()<<"free RAM        : "<<(1.0e-6*info.freeram/info.mem_unit)<<" Mb";
            qDebug()<<"free swap       : "<<(1.0e-6*info.freeswap/info.mem_unit)<<" Mb";
        }
        emit logParameter(LogParameter("systemNrCPUs", QString::number(get_nprocs())+" ", LogParameter::LOG_ONCE));
        emit logParameter(LogParameter("systemUptime", QString::number(info.uptime/3600.)+" h", LogParameter::LOG_LATEST));
        emit logParameter(LogParameter("systemFreeMem", QString::number(1e-6*info.freeram/info.mem_unit)+" Mb", LogParameter::LOG_AVERAGE));
        emit logParameter(LogParameter("systemFreeSwap", QString::number(1e-6*info.freeswap/info.mem_unit)+" Mb", LogParameter::LOG_AVERAGE));
        emit logParameter(LogParameter("systemLoadAvg", QString::number(info.loads[0]*f_load)+" ", LogParameter::LOG_AVERAGE));
    }
    
    // rate buffer debug output
    if ( verbose > 2 ) {
        qDebug() << "GPIO Rate Summary:";
		for (auto signalIt=GPIO_PINMAP.begin(); signalIt!=GPIO_PINMAP.end(); signalIt++) {
            const GPIO_SIGNAL signalId=signalIt->first;
            if (GPIO_SIGNAL_MAP[signalId].direction == DIR_IN )  {
				qDebug()<<GPIO_SIGNAL_MAP[signalId].name
						<<"pin:"<<signalIt->second
						<<"rate:"<<rateBuffer.avgRate( signalIt->second )<<"Hz"
						<<"deadtime"<<rateBuffer.currentDeadtime( signalIt->second ).count()<<"us";
			}
        }
	}
    
//		updateOledDisplay();
}

/*
void Daemon::onUBXReceivedTimeTM2(timespec rising, timespec falling, uint32_t accEst, bool valid, uint8_t timeBase, bool utcAvailable)
{
    long double dts=(falling.tv_sec-rising.tv_sec)*1e9;
    dts+=(falling.tv_nsec-rising.tv_nsec);
    if (dts>0.) {
        checkRescaleHisto(histoMap["UbxEventLength"], dts);
        histoMap["UbxEventLength"].fill(dts);
    }
    long double interval=(rising.tv_sec-lastTimestamp.tv_sec)*1e9;
    interval+=(rising.tv_nsec-lastTimestamp.tv_nsec);
    histoMap["UbxEventInterval"].fill(1e-6*interval);
    lastTimestamp=rising;
}
*/

void Daemon::onUBXReceivedTimeTM2(const UbxTimeMarkStruct& tm) {
    if (!tm.risingValid && !tm.fallingValid) {
		qDebug()<<"Daemon::onUBXReceivedTimeTM2(const UbxTimeMarkStruct&): detected invalid time mark message; no rising or falling edge data";
		return;
	}
    static UbxTimeMarkStruct lastTimeMark { };
	//static int64_t lastPulseLength = 0;
	
    long double dts=(tm.falling.tv_sec-tm.rising.tv_sec)*1.0e9L;
    dts+=(tm.falling.tv_nsec-tm.rising.tv_nsec);
    if ((dts > 0.0L) && tm.fallingValid) {
        checkRescaleHisto(histoMap["UbxEventLength"], static_cast<double>(dts));
        histoMap["UbxEventLength"].fill(static_cast<double>(dts));
    }
    long double interval=(tm.rising.tv_sec-lastTimeMark.rising.tv_sec)*1.0e9L;
    interval+=(tm.rising.tv_nsec-lastTimeMark.rising.tv_nsec);
    histoMap["UbxEventInterval"].fill(static_cast<double>(1.0e-6L * interval));
    uint16_t diffCount=tm.evtCounter-lastTimeMark.evtCounter;
    emit timeMarkIntervalCountUpdate(diffCount, static_cast<double>(interval * 1.0e-9L));
    lastTimeMark=tm;

    // output is: rising falling timeAcc valid timeBase utcAvailable
	std::stringstream tempStream;
	tempStream << tm.rising << tm.falling << tm.accuracy_ns << " " << tm.evtCounter << " " 
		<< static_cast<short>(tm.valid)	<< " " << static_cast<short>(tm.timeBase) << " "
		<< static_cast<short>(tm.utcAvailable);
	emit eventMessage(QString::fromStdString(tempStream.str()));
	
	if (!tm.risingValid || !tm.fallingValid) {
		qDebug() << "detected timemark message with reconstructed edge time ("<<QString((tm.risingValid)?"falling":"rising")<<")";
		qDebug() << "msg:"<<QString::fromStdString(tempStream.str());
	}
	
    TcpMessage tcpMessage(TCP_MSG_KEY::MSG_UBX_TIMEMARK);
    (*tcpMessage.dStream) << tm;
    emit sendTcpMessage(tcpMessage);
}


void Daemon::updateOledDisplay() {
    if (!oled->devicePresent()) return;
    oled->clearDisplay();
    oled->setCursor(0,2);
    oled->print("*Cosmic Shower Det.*\n");
//	oled->printf("rate (XOR) %4.2f 1/s\n", getRateFromCounts(XOR_RATE));
//	oled->printf("rate (AND) %4.2f 1/s\n", getRateFromCounts(AND_RATE));
    oled->printf("Rates %4.1f %4.1f /s\n", getRateFromCounts(AND_RATE), getRateFromCounts(XOR_RATE));
//	oled->printf("temp %4.2f %cC\n", lm75->lastTemperatureValue(), DEGREE_CHARCODE);
    oled->printf("temp %4.2f %cC\n", lm75->getTemperature(), DEGREE_CHARCODE);
    oled->printf("%d(%d) Sats ", nrVisibleSats().toInt(), nrSats().toInt(), DEGREE_CHARCODE);
    oled->printf("%s\n", FIX_TYPE_STRINGS[fixStatus().toInt()].toStdString().c_str());
    oled->display();
}
/*
void Daemon::startRateScan(uint8_t channel) {
    if (!dac->devicePresent()) return;
    // save all the settings which will be altered during the scan
    RateScanInfo *scanInfo = new RateScanInfo;
    scanInfo->origPcaMask = pcaPortMask;
    scanInfo->origEventTrigger = eventTrigger;
    setPcaChannel((channel==0)?MUX_DISC1:MUX_DISC2);
    setEventTriggerSelection(EXT_TRIGGER);
    pigHandler->setInhibited(true);

    quint16 counterStart=propertyMap["events"]().toUInt();
    scanInfo->lastEvtCounter=counterStart;
    scanInfo->minThr=0.002;
    scanInfo->maxThr=0.1;
    scanInfo->currentThr=scanInfo->minThr;
    scanInfo->thrIncrement=0.0005;
    scanInfo->thrChannel=channel;
    scanInfo->nrLoops=(RATE_SCAN_ITERATIONS>0)?RATE_SCAN_ITERATIONS:1;
    scanInfo->currentLoop=0;
    dac->setVoltage(channel, scanInfo->currentThr);
//	usleep(10000UL);
    QTimer::singleShot(RATE_SCAN_TIME_INTERVAL_MS, [this,scanInfo](){
        this->doRateScanIteration(scanInfo);
    } );
}
*/

void Daemon::startRateScan(uint8_t channel) {
    if (!dac->devicePresent() || channel>1) return;
    // save all the settings which will be altered during the scan
/*
    RateScan *scan = new RateScan;
    scan->origPcaMask = pcaPortMask;
    scan->origEventTrigger = eventTrigger;
    scan->origScanPar=dacThresh[channel];
    setPcaChannel((channel==0)?MUX_DISC1:MUX_DISC2);
    setEventTriggerSelection(EXT_TRIGGER);
    pigHandler->setInhibited(true);

    quint16 counterStart=propertyMap["events"]().toUInt();
    scan->lastEvtCounter=counterStart;
    scan->minScanPar=0.002;
    scan->maxScanPar=0.1;
    scan->currentScanPar=scan->minScanPar;
    scan->scanParIncrement=0.0005;
    scanInfo->nrLoops=(RATE_SCAN_ITERATIONS>0)?RATE_SCAN_ITERATIONS:1;
    scanInfo->currentLoop=0;
    dac->setVoltage(channel, scan->currentScanPar);
//	usleep(10000UL);
    QTimer::singleShot(RATE_SCAN_TIME_INTERVAL_MS, [this,scanInfo](){
        this->doRateScanIteration(scanInfo);
    } );
*/
}

void Daemon::doRateScanIteration(RateScanInfo* info) {
    uint currCounter = propertyMap["events"]().toUInt();
    uint diffCount = currCounter-info->lastEvtCounter;
    info->lastEvtCounter = static_cast<quint16>(currCounter);

    float thr=info->currentThr+info->thrIncrement;
    double rate = diffCount * 1000.0 /  MuonPi::Config::Hardware::RateScan::interval/*RATE_SCAN_TIME_INTERVAL_MS*/;
//	qDebug() << "thr"<<(int)info->thrChannel<<"="<<info->currentThr<<": "<<rate<<" Hz";
    qDebug() << info->currentThr<<" "<<rate<<" Hz";
    if (thr<=info->maxThr) {
        // initiate next scan step
        dac->setVoltage(info->thrChannel, thr);
//		usleep(10000UL);
        info->currentThr=thr;
        QTimer::singleShot(MuonPi::Config::Hardware::RateScan::interval/*RATE_SCAN_TIME_INTERVAL_MS*/, [this,info](){
            this->doRateScanIteration(info);
        } );
    } else {
        // scan is done, restore the original settings now
        setDacThresh(0, dacThresh[0]);
        setDacThresh(1, dacThresh[1]);
        setPcaChannel(info->origPcaMask);
        setEventTriggerSelection(info->origEventTrigger);
        pigHandler->setInhibited(false);
        delete info;
    }
}

