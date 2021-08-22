#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include "muondetector_shared_global.h"
#include "tcpmessage.h"

#include <QFile>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <time.h>
#include <vector>

class MUONDETECTORSHARED TcpConnection : public QObject {
    Q_OBJECT

public:
    TcpConnection(QString hostName, quint16 port, int verbose = 0, int timeout = 15000,
        int pingInterval = 5000, QObject* parent = 0);
    TcpConnection(int socketDescriptor, int verbose = 0, int timeout = 15000,
        int pingInterval = 5000, QObject* parent = 0);
    ~TcpConnection();
    void delay(int millisecondsWait);
    const QString& getPeerAddress() const { return peerAddress; }
    quint16 getPeerPort() const { return peerPort; }
    QTcpSocket* getTcpSocket() { return tcpSocket; }
    uint32_t getNrBytesRead() const { return bytesRead; }
    uint32_t getNrBytesWritten() const { return bytesWritten; }
    time_t firstConnectionTime() const { return firstConnection; }

signals:
    void madeConnection(QString remotePeerAddress, quint16 remotePeerPort, QString localAddress, quint16 localPort);
    void connectionTimeout(QString remotePeerAddress, quint16 remotePeerPort, QString localAddress, quint16 localPort,
        quint32 timeoutTime, quint32 connectionDuration);
    void error(int socketError, const QString message);
    void toConsole(QString data);
    void connected();
    void receivedTcpMessage(TcpMessage tcpMessage);
    void finished();

public slots:
    void makeConnection();
    void receiveConnection();
    void closeConnection(QString closedAddress);
    void closeThisConnection();
    void onReadyRead();
    bool sendTcpMessage(TcpMessage tcpMessage);

private:
    bool writeBlock(const QByteArray& block);
    int timeout;
    int verbose;
    int pingInterval;
    int m_socketDescriptor;
    quint16 blockSize = 0;
    QString peerAddress, localAddress;
    QDataStream* in = nullptr;
    QTcpSocket* tcpSocket = nullptr;
    QString hostName;
    quint16 port;
    quint16 peerPort;
    quint16 localPort;
    QPointer<QTimer> t;
    time_t lastConnection;
    time_t firstConnection;
    uint32_t bytesRead = 0, bytesWritten = 0;
};
#endif // TCPCONNECTION_H
