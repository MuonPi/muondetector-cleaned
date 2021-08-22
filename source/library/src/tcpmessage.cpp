#include "tcpmessage.h"
#include "muondetector_structs.h"

#include <QDebug>

TcpMessage::TcpMessage(quint16 tcpMsgID)
{
    m_msgID = tcpMsgID;
    dStream = new QDataStream(&m_data, QIODevice::ReadWrite);
    m_byteCount = 0;
    *dStream << static_cast<quint16>(0);
    *dStream << tcpMsgID;
}

TcpMessage::TcpMessage(QByteArray& rawdata)
    : m_data { rawdata }
{
    dStream = new QDataStream(&m_data, QIODevice::ReadWrite);
    *dStream >> m_byteCount;
    *dStream >> m_msgID;
}

TcpMessage::TcpMessage(TCP_MSG_KEY tcpMsgID)
    : TcpMessage { static_cast<quint16>(tcpMsgID) }
{
}

TcpMessage::~TcpMessage()
{
    delete dStream;
    dStream = nullptr;
}

TcpMessage::TcpMessage(const TcpMessage& tcpMessage)
{
    m_msgID = tcpMessage.getMsgID();
    m_data = tcpMessage.getData();
    dStream = new QDataStream(&m_data, QIODevice::ReadWrite);
    quint16 temp;
    *dStream >> temp;
    *dStream >> temp;
    m_byteCount = tcpMessage.getByteCount();
}

void TcpMessage::setData(QByteArray& rawData)
{
    m_data = rawData;
}

void TcpMessage::setMsgID(quint16 tcpMsgID)
{
    m_msgID = tcpMsgID;
}

const QByteArray& TcpMessage::getData() const
{
    return m_data;
}

quint16 TcpMessage::getMsgID() const
{
    return m_msgID;
}

quint16 TcpMessage::getByteCount() const
{
    return m_byteCount;
}
