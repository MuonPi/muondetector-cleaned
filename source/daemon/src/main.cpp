#include <signal.h>
#include <stdio.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QObject>
#include <QDateTime>
#include <iostream>
#include <libconfig.h++>
#include <termios.h>
#include <unistd.h>

#include <config.h>
#include "utility/custom_io_operators.h"
#include "daemon.h"
#include <gpio_pin_definitions.h>

static const char* CONFIG_FILE = MuonPi::Config::file;
static int verbose = 0;

[[nodiscard]] auto getch() -> int;
[[nodiscard]] auto getpass(const char* prompt, bool show_asterisk) -> std::string;

auto getch() -> int
{
    int ch;
    struct termios t_old, t_new;

    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

auto getpass(const char* prompt, bool show_asterisk) -> std::string
{
    const char BACKSPACE = 127;
    const char RETURN = 10;

    std::string password;
    unsigned char ch = 0;
    ch = getch();
    std::cout << prompt << '\n'
              << std::flush;
    while ((ch = getch()) != RETURN) {
        if (ch == BACKSPACE) {
            if (password.length() != 0) {
                if (show_asterisk)
                    std::cout << "\b \b";
                password.resize(password.length() - 1);
            }
        } else {
            password += ch;
            if (show_asterisk)
                std::cout << '*';
        }
    }
    std::cout << '\n'
              << std::flush;
    return password;
}

// The application's custom message handler
// All outputs to console should go through the QT message mechanism:
// i.e. qDebug(), qWarning(), qCritical(), qFatal()
void messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        if (verbose) {
            fprintf(stdout, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
            fflush(stdout);
        }
        break;
    case QtInfoMsg:
        fprintf(stdout, "Info: %s\n", localMsg.constData());
        fflush(stdout);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s\n", localMsg.constData());
        fflush(stderr);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", localMsg.constData());
        fflush(stderr);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        fflush(stderr);
        break;
    }
}

int main(int argc, char* argv[])
{
    qRegisterMetaType<TcpMessage>("TcpMessage");
    qInstallMessageHandler(messageOutput);
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("muondetector-daemon");
    QCoreApplication::setApplicationVersion(QString::fromStdString(MuonPi::Version::software.string()));
    // config file handling
    libconfig::Config cfg;

	qInfo()	<< "MuonPi Muondetector Daemon "
			<< "V"+QString::fromStdString(MuonPi::Version::software.string())
			<< "(build "+ QString(__TIMESTAMP__) +")";
			
    // Read the file. If there is an error, report it and exit.
    try {
        cfg.readFile(MuonPi::Config::file);
    } catch (const libconfig::FileIOException& fioex) {
        qWarning() << "Error while reading config file" << QString(CONFIG_FILE);
    } catch (const libconfig::ParseException& pex) {
        qFatal(qPrintable("Parse error at " + QString(pex.getFile()) + " : line " + QString(pex.getLine()) + " - " + QString(pex.getError())));
        return (EXIT_FAILURE);
    }

    // command line input management
    QCommandLineParser parser;
    parser.setApplicationDescription("MuonPi cosmic shower muon detector control and configuration program (daemon)\n"
                                     "with added tcp implementation for synchronisation of "
                                     "data with a central server through MQTT protocol");
    parser.addHelpOption();
    parser.addVersionOption();

    // add module path for example /dev/gps0 or /dev/ttyAMA0
    parser.addPositionalArgument("device", QCoreApplication::translate("main", "path to u-blox GNSS device\n"
                                                                               "e.g. /dev/ttyAMA0"));

    // login option
    QCommandLineOption mqttLoginOption(QStringList() << "l"
                                                     << "login",
        QCoreApplication::translate("main", "ask for login to the online mqtt server"));
    parser.addOption(mqttLoginOption);

    // station ID (some name for individual stations if someone has multiple)
    QCommandLineOption stationIdOption("id",
        QCoreApplication::translate("main", "set station ID"),
        QCoreApplication::translate("main", "ID"));
    parser.addOption(stationIdOption);

    // verbosity option
    QCommandLineOption verbosityOption(QStringList() << "e"
                                                     << "verbose",
        QCoreApplication::translate("main", "set verbosity level\n"
                                            "5 is max"),
        QCoreApplication::translate("main", "verbosity"));
    parser.addOption(verbosityOption);

    // dumpraw option
    QCommandLineOption dumpRawOption("d",
        QCoreApplication::translate("main", "dump raw gps device (NMEA) output to stdout"));
    parser.addOption(dumpRawOption);

    // show GNSS configs
    QCommandLineOption showGnssConfigOption("c",
        QCoreApplication::translate("main", "configure standard ubx protocol messages at start"));
    parser.addOption(showGnssConfigOption);

    // show outgoing ubx messages as hex
    QCommandLineOption showoutOption(QStringList() << "showoutgoing"
                                                   << "showout",
        QCoreApplication::translate("main", "show outgoing ubx messages as hex"));
    parser.addOption(showoutOption);

    // show incoming ubx messages as hex
    QCommandLineOption showinOption(QStringList() << "showincoming"
                                                  << "showin",
        QCoreApplication::translate("main", "show incoming ubx messages as hex"));
    parser.addOption(showinOption);

    // peerAddress option
    QCommandLineOption peerIpOption(QStringList() << "peer"
                                                  << "peerAddress",
        QCoreApplication::translate("main", "set file server ip address"),
        QCoreApplication::translate("main", "peerAddress"));
    parser.addOption(peerIpOption);

    // peerPort option
    QCommandLineOption peerPortOption(QStringList() << "pp"
                                                    << "peerPort",
        QCoreApplication::translate("main", "set file server port"),
        QCoreApplication::translate("main", "peerPort"));
    parser.addOption(peerPortOption);

    // daemonAddress option
    QCommandLineOption daemonIpOption(QStringList() << "server"
                                                    << "daemonAddress",
        QCoreApplication::translate("main", "set gui server ip address"),
        QCoreApplication::translate("main", "daemonAddress"));
    parser.addOption(daemonIpOption);

    // daemonPort option
    QCommandLineOption daemonPortOption(QStringList() << "dp"
                                                      << "daemonPort",
        QCoreApplication::translate("main", "set gui server port"),
        QCoreApplication::translate("main", "daemonPort"));
    parser.addOption(daemonPortOption);

    // baudrate option
    QCommandLineOption baudrateOption("b",
        QCoreApplication::translate("main", "set baudrate for serial connection"),
        QCoreApplication::translate("main", "baudrate"));
    parser.addOption(baudrateOption);

    // discriminator thresholds:
    QCommandLineOption discr1Option(QStringList() << "discr1"
                                                  << "thresh1"
                                                  << "th1",
        QCoreApplication::translate("main",
            "set discriminator 1 threshold in Volts"),
        QCoreApplication::translate("main", "threshold1"));
    parser.addOption(discr1Option);
    QCommandLineOption discr2Option(QStringList() << "discr2"
                                                  << "thresh2"
                                                  << "th2",
        QCoreApplication::translate("main",
            "set discriminator 2 threshold in Volts"),
        QCoreApplication::translate("main", "threshold2"));
    parser.addOption(discr2Option);

    // pcaPortMask to select signal to ublox
    QCommandLineOption pcaPortMaskOption(QStringList() << "pca"
                                                       << "signal",
        QCoreApplication::translate("main", "set input signal for ublox interrupt pin:"
                                            "\n0 - coincidence (AND)"
                                            "\n1 - anti-coincidence (XOR)"
                                            "\n2 - discr 1"
                                            "\n3 - discr 2"
                                            "\n4 - vcc"
                                            "\n5 - timepulse"
                                            "\n6 - N/A"
                                            "\n7 - ext signal"),
        QCoreApplication::translate("main", "channel"));
    parser.addOption(pcaPortMaskOption);

    // biasVoltage for SiPM
    QCommandLineOption biasVoltageOption(QStringList() << "bias"
                                                       << "vout",
        QCoreApplication::translate("main", "set bias voltage for SiPM"),
        QCoreApplication::translate("main", "bias voltage"));
    parser.addOption(biasVoltageOption);

    // biasVoltage on or off
    QCommandLineOption biasPowerOnOff(QStringList() << "p",
        QCoreApplication::translate("main", "bias voltage on or off"));
    parser.addOption(biasPowerOnOff);

    // preamps on:
    QCommandLineOption preamp1Option(QStringList() << "pre1"
                                                   << "preamp1",
        QCoreApplication::translate("main", "preamplifier channel 1 on/off (0/1)"));
    parser.addOption(preamp1Option);
    QCommandLineOption preamp2Option(QStringList() << "pre2"
                                                   << "preamp2",
        QCoreApplication::translate("main", "preamplifier channel 2 on/off (0/1)"));
    parser.addOption(preamp2Option);

    // gain:
    QCommandLineOption gainOption(QStringList() << "g"
                                                << "gain",
        QCoreApplication::translate("main", "peak detector high gain select"));
    parser.addOption(gainOption);

    // event trigger for ADC
    QCommandLineOption eventInputOption(QStringList() << "t"
                                                      << "trigger",
        QCoreApplication::translate("main", "event (trigger) signal input:"
                                            "\n0 - coincidence (AND)"
                                            "\n1 - anti-coincidence (XOR)"),
        QCoreApplication::translate("main", "trigger"));
    parser.addOption(eventInputOption);

    // input polarity switch:
    QCommandLineOption pol1Option(QStringList() << "pol1"
                                                << "polarity1",
        QCoreApplication::translate("main", "input polarity ch1 negative (0) or positive (1)"));
    parser.addOption(pol1Option);
    QCommandLineOption pol2Option(QStringList() << "pol2"
                                                << "polarity2",
        QCoreApplication::translate("main", "input polarity ch2 negative (0) or positive (1)"));
    parser.addOption(pol2Option);

    // process the actual command line arguments given by the user
    parser.process(a);
    const QStringList args = parser.positionalArguments();
    if (args.size() > 1) {
        qWarning() << "you set additional positional arguments but the program does not use them";
    }

    Daemon::configuration daemonConfig;

    bool ok;
    if (parser.isSet(verbosityOption)) {
        verbose = parser.value(verbosityOption).toInt(&ok);
        if (!ok) {
            verbose = 0;
            qWarning() << "wrong verbosity level selection...setting to 0";
        }
    }
    daemonConfig.verbose = verbose;

    try {
        daemonConfig.maxGeohashLength = cfg.lookup("max_geohash_length");
    } catch (const libconfig::SettingNotFoundException&) {
    }

    try {
        daemonConfig.storeLocal = cfg.lookup("store_local");
    } catch (const libconfig::SettingNotFoundException&) {
    }

    // setup all variables for ublox module manager, then make the object run
    if (!args.empty() && args.at(0) != "") {
        daemonConfig.gpsdevname = args.at(0);
    } else
        try {
            std::string gpsdevnameCfg = cfg.lookup("ublox_device");
            if (verbose > 2)
                std::cout << "ublox device: " << gpsdevnameCfg << std::endl;
            daemonConfig.gpsdevname = QString::fromStdString(gpsdevnameCfg);
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'ublox_device' setting in configuration file. Will guess...";
            QDir directory("/dev", "*", QDir::Name, QDir::System);
            QStringList serialports = directory.entryList(QStringList({ "ttyS0", "ttyAMA0", "serial0" }));
            if (!serialports.empty()) {
                daemonConfig.gpsdevname = QString("/dev/" + serialports.last());
                qInfo() << "detected" << daemonConfig.gpsdevname << "as most probable candidate";
            } else {
                qCritical() << "no device selected, will not connect to GNSS module" << endl;
            }
        }

    if (verbose > 4) {
        qDebug() << "int main running in thread"
                 << QString("0x%1").arg(reinterpret_cast<std::uint64_t>(QCoreApplication::instance()->thread()));
    }
    daemonConfig.dumpRaw = parser.isSet(dumpRawOption);
    if (parser.isSet(baudrateOption)) {
        daemonConfig.baudrate = parser.value(baudrateOption).toInt(&ok);
        if (!ok || daemonConfig.baudrate < 0) {
            daemonConfig.baudrate = 9600;
            qWarning() << "wrong input for baudrate...using default:" << daemonConfig.baudrate;
        }
    } else
        try {
            int baudrateCfg = cfg.lookup("ublox_baud");
            if (verbose > 2)
                qInfo() << "ublox baudrate:" << baudrateCfg;
            daemonConfig.baudrate = baudrateCfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            if (verbose > 1)
                qWarning() << "No 'ublox_baud' setting in configuration file. Assuming" << daemonConfig.baudrate;
        }

    daemonConfig.configGnss = parser.isSet(showGnssConfigOption);
    if (parser.isSet(peerPortOption)) {
        daemonConfig.peerPort = parser.value(peerPortOption).toUInt(&ok);
        if (!ok) {
            daemonConfig.peerPort = 0;
            qCritical() << "wrong input peerPort (maybe not an integer)";
        }
    }
    if (parser.isSet(peerIpOption)) {
        daemonConfig.peerAddress = parser.value(peerIpOption);
        if (!QHostAddress(daemonConfig.peerAddress).toIPv4Address()) {
            if (daemonConfig.peerAddress != "localhost" && daemonConfig.peerAddress != "local") {
                daemonConfig.peerAddress = "";
                qCritical() << "wrong input ipAddress, not an ipv4address";
            }
        }
    }
    try {
        int port = cfg.lookup("tcp_port");
        daemonConfig.serverPort = static_cast<quint16>(port);
    } catch (const libconfig::SettingNotFoundException&) {
    }
    if (parser.isSet(daemonPortOption)) {
        daemonConfig.serverPort = parser.value(daemonPortOption).toUInt(&ok);
        if (!ok) {
            daemonConfig.peerPort = 0;
            qCritical() << "wrong input peerPort (maybe not an integer)";
        }
    }
    if (parser.isSet(daemonIpOption)) {
        daemonConfig.serverAddress = parser.value(daemonIpOption);
        if (!QHostAddress(daemonConfig.serverAddress).toIPv4Address()) {
            if (daemonConfig.serverAddress != "localhost" && daemonConfig.serverAddress != "local") {
                daemonConfig.serverAddress = "";
                qCritical() << "wrong daemon ipAddress, not an ipv4address";
            }
        }
    }

    if (parser.isSet(pcaPortMaskOption)) {
        daemonConfig.pcaPortMask = parser.value(pcaPortMaskOption).toUInt(&ok);
        if (!ok) {
            daemonConfig.pcaPortMask = 0;
            qCritical() << "wrong input pcaPortMask (maybe not an unsigned integer)";
        }
    } else
        try {
            int pcaPortMaskCfg = cfg.lookup("timing_input");
            if (verbose > 2)
                qDebug() << "timing input: " << pcaPortMaskCfg << endl;
            daemonConfig.pcaPortMask = pcaPortMaskCfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'timing_input' setting in configuration file. Assuming" << (int)daemonConfig.pcaPortMask;
        }

    daemonConfig.showout = parser.isSet(showoutOption);
    daemonConfig.showin = parser.isSet(showinOption);

    if (parser.isSet(discr1Option)) {
        daemonConfig.dacThresh[0] = parser.value(discr1Option).toFloat(&ok);
        if (!ok) {
            daemonConfig.dacThresh[0] = -1.;
            qCritical() << "error in value for discr1 (maybe not a float)";
        }
    }
    if (parser.isSet(discr2Option)) {
        daemonConfig.dacThresh[1] = parser.value(discr2Option).toFloat(&ok);
        if (!ok) {
            daemonConfig.dacThresh[1] = -1.;
            qCritical() << "error in value for discr2 (maybe not a float)";
        }
    }
    if (parser.isSet(biasVoltageOption)) {
        daemonConfig.biasVoltage = parser.value(biasVoltageOption).toFloat(&ok);
        if (!ok) {
            daemonConfig.biasVoltage = -1.;
            qCritical() << "error in value for biasVoltage (maybe not a float)";
        }
    }

    if (parser.isSet(biasPowerOnOff)) {
        daemonConfig.bias_ON = true;
    } else
        try {
            int biasPowerCfg = cfg.lookup("bias_switch");
            if (verbose > 2)
                qDebug() << "bias switch:" << biasPowerCfg;
            daemonConfig.bias_ON = biasPowerCfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'bias_switch' setting in configuration file. Assuming" << (int)daemonConfig.bias_ON;
        }

    if (parser.isSet(preamp1Option)) {
        daemonConfig.preamp[0] = true;
    } else
        try {
            int preamp1Cfg = cfg.lookup("preamp1_switch");
            if (verbose > 2)
                qDebug() << "preamp1 switch:" << preamp1Cfg;
            daemonConfig.preamp[0] = preamp1Cfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'preamp1_switch' setting in configuration file. Assuming" << (int)daemonConfig.preamp[0];
        }

    if (parser.isSet(preamp2Option)) {
        daemonConfig.preamp[1] = true;
    } else
        try {
            int preamp2Cfg = cfg.lookup("preamp2_switch");
            if (verbose > 2)
                qDebug() << "preamp2 switch:" << preamp2Cfg;
            daemonConfig.preamp[1] = preamp2Cfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'preamp2_switch' setting in configuration file. Assuming " << (int)daemonConfig.preamp[1];
        }

    if (parser.isSet(gainOption)) {
        daemonConfig.gain = true;
    } else {
        try {
            int gainCfg = cfg.lookup("gain_switch");
            if (verbose > 2)
                qDebug() << "gain switch:" << gainCfg;
            daemonConfig.gain = gainCfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            if (verbose > 0)
                qWarning() << "No 'gain_switch' setting in configuration file. Assuming" << (int)daemonConfig.gain;
        }
    }

    if (parser.isSet(eventInputOption)) {
        daemonConfig.eventTrigger = parser.value(eventInputOption).toUInt(&ok);
        if (!ok || daemonConfig.eventTrigger > 1) {
            qCritical() << "wrong trigger input signal (valid: 0,1)";
            return -1;
        } else {
            switch (daemonConfig.eventTrigger) {
            case 1:
                daemonConfig.eventTrigger = EVT_AND;
                break;
            case 0:
            default:
                daemonConfig.eventTrigger = EVT_XOR;
                break;
            }
        }
    } else
        try {
            int eventTriggerCfg = cfg.lookup("trigger_input");
            if (verbose > 2)
                qDebug() << "event trigger : " << eventTriggerCfg;
            switch (eventTriggerCfg) {
            case 1:
                daemonConfig.eventTrigger = EVT_AND;
                break;
            case 0:
            default:
                daemonConfig.eventTrigger = EVT_XOR;
                break;
            }
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'trigger_input' setting in configuration file. Assuming signal" << GPIO_SIGNAL_MAP[(GPIO_PIN)daemonConfig.eventTrigger].name;
        }

    if (parser.isSet(pol1Option)) {
        unsigned int pol1int = parser.value(pol1Option).toUInt(&ok);
        if (!ok || pol1int > 1) {
            qCritical() << "wrong input polarity setting ch1 (valid: 0,1)";
            return -1;
        } else {
            daemonConfig.polarity[0] = (bool)pol1int;
        }
    } else {
        try {
            int pol1Cfg = cfg.lookup("input1_polarity");
            if (verbose > 2)
                qDebug() << "input polarity ch1:" << pol1Cfg;
            daemonConfig.polarity[0] = (bool)pol1Cfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            if (verbose > 0)
                qWarning() << "No 'input1_polarity' setting in configuration file. Assuming" << (int)daemonConfig.polarity[0];
        }
    }

    if (parser.isSet(pol2Option)) {
        unsigned int pol2int = parser.value(pol2Option).toUInt(&ok);
        if (!ok || pol2int > 1) {
            qCritical() << "wrong input polarity setting ch2 (valid: 0,1)";
            return -1;
        } else {
            daemonConfig.polarity[1] = (bool)pol2int;
        }
    } else {
        try {
            int pol2Cfg = cfg.lookup("input2_polarity");
            if (verbose > 2)
                qDebug() << "input polarity ch2:" << pol2Cfg;
            daemonConfig.polarity[1] = (bool)pol2Cfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            if (verbose > 0)
                qWarning() << "No 'input2_polarity' setting in configuration file. Assuming" << (int)daemonConfig.polarity[1];
        }
    }

    std::string username = "";
    std::string password = "";
    if (parser.isSet(mqttLoginOption)) {
        std::cout << "To set the login for the mqtt-server, please enter user name:\n"
                  << std::flush;
        std::cin >> username;
        password = getpass("please enter password:", true);
    } else
        try {
            std::string userNameCfg = cfg.lookup("mqtt_user");
            std::string passwordCfg = cfg.lookup("mqtt_password");
            if (verbose > 2)
                qDebug() << "mqtt user: " << QString::fromStdString(userNameCfg) << " passw: " << QString::fromStdString(passwordCfg);
            username = userNameCfg;
            password = passwordCfg;
        } catch (const libconfig::SettingNotFoundException& nfex) {
            if (verbose > 1)
                qDebug() << "No 'mqtt_user' or 'mqtt_password' setting in configuration file. Will continue with previously stored credentials";
        }

    if (parser.isSet(stationIdOption)) {
        daemonConfig.station_ID = parser.value(stationIdOption);
    } else {
        // Get the station id from config, if it exists
        try {
            std::string stationIdString = cfg.lookup("stationID");
            if (verbose)
                qInfo() << "station id: " << QString::fromStdString(stationIdString);
            daemonConfig.station_ID = QString::fromStdString(stationIdString);
        } catch (const libconfig::SettingNotFoundException& nfex) {
            qWarning() << "No 'stationID' setting in configuration file. Assuming stationID='0'";
        }
    }

    Daemon daemon { daemonConfig };

    return a.exec();
}
