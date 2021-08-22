#ifndef FILEHANDLER_H
#define FILEHANDLER_H
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QObject>
#include <QQueue>
#include <QStandardPaths>
#include <QVector>
#include "logparameter.h"

class FileHandler : public QObject {
    Q_OBJECT

public:
    FileHandler(const QString& userName, const QString& passWord, quint32 fileSizeMB = 500, QObject* parent = nullptr);
    QString getCurrentDataFileName() const;
    QString getCurrentLogFileName() const;
    QFileInfo dataFileInfo() const;
    QFileInfo logFileInfo() const;
    qint64 currentLogAge();

signals:
    void logIntervalSignal();
    void mqttConnect(QString username, QString password);
    void logRotateSignal();

public slots:
    void start();
    void writeToDataFile(const QString& data); // writes data to the file opened in "dataFile"
    void writeToLogFile(const QString& log); // writes log data to the file opened in "logFile"

private slots:
    void onUploadRemind();

private:
    // save and send data everyday
    QFile* dataFile = nullptr; // the file date is currently written to. (timestamps)
    QFile* logFile = nullptr; // the file log information is written to.
    QString hashedMacAddress;
    QString configFilePath;
    QString loginDataFilePath;
    QString configPath;
    QString dataFolderPath;
    QString currentWorkingFilePath;
    QString currentWorkingLogPath;
    QString username;
    QString password;
    QFlags<QFileDevice::Permission> defaultPermissions = QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::ReadGroup | QFileDevice::ReadOther;
    QStringList notUploadedFilesNames;
    bool saveLoginData(QString username, QString password);
    bool readLoginData();
    bool openFiles(bool writeHeader = false); // reads the config file and opens the correct data file to write to
    bool readFileInformation();
    bool uploadDataFile(QString fileName); // sends a data file with some filename via lftp script to the server
    bool uploadRecentDataFiles();
    bool switchFiles(QString fileName = ""); // closes the old file and opens a new one, changing "dataConfig.conf" to the new file
    bool writeConfigFile();
    void closeFiles();
    QString createFileName(); // creates a fileName based on date time and mac address
    quint32 fileSize; // in MB
    QDateTime lastUploadDateTime;
    QTime dailyUploadTime;
};

#endif // FILEHANDLER_H
