#ifndef TCPMESSAGE_H
#define TCPMESSAGE_H

#include "muondetector_shared_global.h"

#include <QByteArray>
#include <QDataStream>

enum class TCP_MSG_KEY : quint16;

// how is a message coded in TcpMessage?
// in the data QByteArray:
// at pos 0: length of message (quint16) -> length of QByteArray - length of this number (sizeof(quint16))
// at pos 1: tcpMsgID (quint16), shows what kind of message it is

class MUONDETECTORSHARED TcpMessage {
public:
    TcpMessage(quint16 tcpMsgID = 0);
    TcpMessage(TCP_MSG_KEY tcpMsgID);
    TcpMessage(QByteArray& rawdata);
    TcpMessage(const TcpMessage& tcpMessage);
    ~TcpMessage();

    void setMsgID(quint16 tcpMsgID);
    void setData(QByteArray& data);
    const QByteArray& getData() const;
    quint16 getMsgID() const;
    quint16 getByteCount() const;

    QDataStream* dStream = nullptr;

private:
    QByteArray m_data {};
    quint16 m_msgID {};
    quint16 m_byteCount {};
};

#endif // TCPMESSAGE_H
