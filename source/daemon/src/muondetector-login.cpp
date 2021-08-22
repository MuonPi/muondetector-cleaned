#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkConfigurationManager>
#include <QNetworkInterface>
#include <QNetworkSession>
#include <QtGlobal>
#include <crypto++/aes.h>
#include <crypto++/filters.h>
#include <crypto++/hex.h>
#include <crypto++/modes.h>
#include <crypto++/osrng.h>
#include <crypto++/sha.h>
#include <iostream>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "mqtthandler.h"

[[nodiscard]] auto getch() -> char;
[[nodiscard]] auto getpass(const char* prompt, const bool show_asterisk)
    -> std::string;
[[nodiscard]] auto SHA256HashString(const std::string& aString) -> std::string;
[[nodiscard]] auto getMacAddress() -> QString;
[[nodiscard]] auto getMacAddressByteArray() -> QByteArray;
[[nodiscard]] auto saveLoginData(const QString& loginFilePath,
    const QString& username,
    const QString& password) -> bool;

auto getch() -> char
{
    termios t_old {};
    termios t_new {};

    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    char ch { static_cast<char>(getchar()) };

    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

auto getpass(const char* prompt, const bool show_asterisk) -> std::string
{
    const char BACKSPACE = 127;
    const char RETURN = 10;

    std::string password {};
    char ch { getch() };
    std::cout << prompt;
    while ((ch = getch()) != RETURN) {
        if (ch == BACKSPACE) {
            if (password.length() != 0) {
                if (show_asterisk)
                    std::cout << "\b \b";
                password.resize(password.length() - 1);
            }
        } else {
            password += ch;
            if (show_asterisk) {
                std::cout << '*';
            }
        }
    }
    std::cout << std::endl;
    return password;
}

auto SHA256HashString(const std::string& aString) -> std::string
{
    std::string digest {};
    CryptoPP::SHA256 hash {};

    CryptoPP::StringSource foo(
        aString, true,
        new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(digest)));
    return digest;
}

auto getMacAddress() -> QString
{
    QNetworkConfiguration nc {};
    QNetworkConfigurationManager ncm {};
    QList<QNetworkConfiguration> configsForEth {};
    QList<QNetworkConfiguration> configsForWLAN {};
    QList<QNetworkConfiguration> allConfigs {};
    // getting all the configs we can
    foreach (nc, ncm.allConfigurations(QNetworkConfiguration::Active)) {
        if (nc.type() == QNetworkConfiguration::InternetAccessPoint) {
            // selecting the bearer type here
            if (nc.bearerType() == QNetworkConfiguration::BearerWLAN) {
                configsForWLAN.append(nc);
            }
            if (nc.bearerType() == QNetworkConfiguration::BearerEthernet) {
                configsForEth.append(nc);
            }
        }
    }
    // further in the code WLAN's and Eth's were treated differently
    allConfigs.append(configsForWLAN);
    allConfigs.append(configsForEth);
    QString MAC {};
    foreach (nc, allConfigs) {
        QNetworkSession networkSession(nc);
        QNetworkInterface netInterface = networkSession.interface();
        // these last two conditions are for omiting the virtual machines' MAC
        // works pretty good since no one changes their adapter name
        if (!(netInterface.flags() & QNetworkInterface::IsLoopBack) && !netInterface.humanReadableName().toLower().contains("vmware") && !netInterface.humanReadableName().toLower().contains("virtual")) {
            MAC = QString(netInterface.hardwareAddress());
            break;
        }
    }
    return MAC;
}

auto getMacAddressByteArray() -> QByteArray
{
    QString mac { getMacAddress() };
    QByteArray byteArray {};
    byteArray.append(mac);
    return byteArray;
}

auto saveLoginData(const QString& loginFilePath,
    const QString& username,
    const QString& password) -> bool
{
    using namespace CryptoPP;
    QFile loginDataFile { loginFilePath };
    loginDataFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!loginDataFile.open(QIODevice::ReadWrite)) {
        qDebug() << "could not open login data save file";
        return false;
    }
    loginDataFile.resize(0);

    AutoSeededRandomPool rnd {};
    std::string plainText { QString(username + ";" + password).toStdString() };
    std::string keyText {};
    std::string encrypted {};

    keyText = SHA256HashString(getMacAddress().toStdString());
    SecByteBlock key { reinterpret_cast<const byte*>(keyText.data()),
        keyText.size() };

    // Generate a random IV
    SecByteBlock iv { AES::BLOCKSIZE };
    rnd.GenerateBlock(iv, iv.size());

    //////////////////////////////////////////////////////////////////////////
    // Encrypt

    CFB_Mode<AES>::Encryption cfbEncryption {};
    cfbEncryption.SetKeyWithIV(key, key.size(), iv, iv.size());

    StringSource encryptor {
        plainText, true,
        new StreamTransformationFilter(cfbEncryption, new StringSink(encrypted))
    };
    loginDataFile.write(reinterpret_cast<const char*>(iv.data()),
        static_cast<qint64>(iv.size()));
    loginDataFile.write(encrypted.c_str());
    return true;
}

int main()
{
    std::cout << "To set the login for the mqtt-server, please enter your credentials.\nusername: ";
    std::string username {};
    std::cin >> username;
    std::string password { getpass("password: ", true) };
    QString hashedMacAddress { QString { QCryptographicHash::hash(getMacAddressByteArray(), QCryptographicHash::Sha224).toHex() } };
    QDir temp;
    QString saveDirPath { "/var/muondetector/" + hashedMacAddress };
    if (!temp.exists(saveDirPath)) {
        temp.mkpath(saveDirPath);
        if (!temp.exists(saveDirPath)) {
            qFatal(QString("Could not create folder " + saveDirPath + ". Make sure the program is started with the correct permissions.").toStdString().c_str());
        }
    }
    if (!saveLoginData(QString { saveDirPath + "/loginData.save" }, QString::fromStdString(username), QString::fromStdString(password))) {
        return 1;
    }
    MuonPi::MqttHandler mqttHandler { "" };
    std::unique_ptr<QObject> context { new QObject };
    QObject::connect(&mqttHandler, &MuonPi::MqttHandler::mqttConnectionStatus,
        [context = std::move(context)](bool connected) mutable {
            if (connected) {
                std::cout << "login data is correct!\n";
            }
            context.release();
        });
    mqttHandler.start(QString::fromStdString(username), QString::fromStdString(password));
    mqttHandler.mqttDisconnect();
    return 0;
}
