#include "mqtthandler.h"


#include <QDebug>
#include <QTimer>
#include <QThread>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/syscall.h>
#endif
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <string>
#include <algorithm>

#include <iostream>

namespace MuonPi {

void wrapper_callback_connected(mosquitto* /*mqtt*/, void* object, int result)
{
    reinterpret_cast<MqttHandler*>(object)->callback_connected(result);
}

void wrapper_callback_disconnected(mosquitto* /*mqtt*/, void* object, int result)
{
    reinterpret_cast<MqttHandler*>(object)->callback_disconnected(result);
}

void wrapper_callback_message(mosquitto* /*mqtt*/, void* object, const mosquitto_message* message)
{
    reinterpret_cast<MqttHandler*>(object)->callback_message(message);
}

void MqttHandler::callback_connected(int result)
{
    if (result == 1) {
        qWarning() << "Mqtt connection failed: Wrong protocol version";
        set_status(Status::Error);
    } else if (result == 2) {
        qWarning() << "Mqtt connection failed: Credentials rejected";
        set_status(Status::Error);
    } else if (result == 3) {
        qWarning() << "Mqtt connection failed: Broker unavailable";
        set_status(Status::Error);
    } else if (result > 3) {
        qWarning() << "Mqtt connection failed: Other reason";
    } else if (result == 0) {
        qInfo() << "Connected to mqtt.";
        set_status(Status::Connected);
        m_tries = 0;
        emit request_timer_stop();
        return;
    }
    emit request_timer_start(Config::MQTT::timeout * m_tries);
}

void MqttHandler::callback_disconnected(int result)
{
    if (result != 0) {
        qWarning() << "Mqtt disconnected unexpectedly: " + QString::number(result);
        set_status(Status::Error);
        m_tries = 1;
        emit request_timer_start(Config::MQTT::timeout * m_tries);
    } else {
        set_status(Status::Disconnected);
    }

}

void MqttHandler::callback_message(const mosquitto_message* message)
{
    emit receivedMessage(message->topic, reinterpret_cast<char*>(message->payload));
}

void MqttHandler::set_status(Status status)
{
    if (m_status != status) {
        if (status == Status::Connected) {
            emit mqttConnectionStatus(true);
        } else if ((status == Status::Disconnected) || (status == Status::Error)) {
            emit mqttConnectionStatus(false);
        }
    }
    m_status = status;
}

MqttHandler::MqttHandler(const QString& station_id, const int verbosity)
    : m_station_id { station_id.toStdString() }
    , m_verbose { verbosity }
{
    m_reconnect_timer.setInterval(Config::MQTT::timeout);
    m_reconnect_timer.setSingleShot(true);
    connect(&m_reconnect_timer, &QTimer::timeout, this, [this](){mqttConnect();});
    connect(this, &MqttHandler::request_timer_stop, &m_reconnect_timer, &QTimer::stop);
    connect(this, &MqttHandler::request_timer_restart, this, &MqttHandler::timer_restart);
    connect(this, &MqttHandler::request_timer_start, this, &MqttHandler::timer_start);
}

MqttHandler::~MqttHandler()
{
    mqttDisconnect();
    cleanup();
}

void MqttHandler::timer_restart(int timeout)
{
    m_reconnect_timer.stop();
    m_reconnect_timer.start(timeout);
}

void MqttHandler::timer_start(int timeout)
{
    m_reconnect_timer.start(timeout);
}

void MqttHandler::start(const QString& username, const QString& password){
    m_username = username.toStdString();
    m_password = password.toStdString();

    m_data_topic = Config::MQTT::data_topic + m_username + "/" + m_station_id;
    m_log_topic =  Config::MQTT::log_topic + m_username + "/" + m_station_id;

    CryptoPP::SHA1 sha1;
    std::string source = username.toStdString()+m_station_id;  //This will be randomly generated somehow
    m_client_id = "";
    CryptoPP::StringSource{source, true, new CryptoPP::HashFilter(sha1, new CryptoPP::HexEncoder(new CryptoPP::StringSink(m_client_id)))};

    initialise(m_client_id);


    mqttConnect();
}

void MqttHandler::mqttConnect(){
    if (connected()) {
        qDebug() << "Already connected to Mqtt.";
        return;
    }
    qDebug() << "Trying to connect to MQTT.";

    if (m_mqtt == nullptr) {
        initialise(m_client_id);
    }

    m_tries++;
    set_status(Status::Connecting);

    if (m_tries > s_max_tries) {
        set_status(Status::Error);
        qCritical() << "Giving up trying to connect to MQTT after too many tries";
        return;
    }

    auto result { mosquitto_username_pw_set(m_mqtt, m_username.c_str(), m_password.c_str()) };
    if (result != MOSQ_ERR_SUCCESS) {
        qWarning() << "Error when setting username and password: " + QString{ strerror(result) };
        return;
    }
    result = mosquitto_connect(m_mqtt, Config::MQTT::host, Config::MQTT::port, 60);
    if (result == MOSQ_ERR_SUCCESS) {
        return;
    }
    qWarning() << "Could not connect to MQTT: " + QString { strerror(result) } + ". Trying again in " + QString::number(Config::MQTT::timeout * m_tries) + "ms";
    emit request_timer_start(Config::MQTT::timeout * m_tries);
}

void MqttHandler::mqttDisconnect(){
    if (!connected()) {
        return;
    }

    auto result { mosquitto_disconnect(m_mqtt) };
    if (result == MOSQ_ERR_SUCCESS) {
        return;
    }
    qWarning() << "Could not disconnect from Mqtt: " + QString{ strerror(result) };
}

auto MqttHandler::connected() -> bool{
    return (m_mqtt != nullptr) && (m_status == Status::Connected);
}

void MqttHandler::initialise(const std::string& client_id)
{
    if (m_mqtt != nullptr) {
        cleanup();
    }

    mosquitto_lib_init();

    m_mqtt = mosquitto_new(client_id.c_str(), true, this);

    mosquitto_connect_callback_set(m_mqtt, wrapper_callback_connected);
    mosquitto_disconnect_callback_set(m_mqtt, wrapper_callback_disconnected);
    mosquitto_message_callback_set(m_mqtt, wrapper_callback_message);

    mosquitto_loop_start(m_mqtt);
}

void MqttHandler::cleanup() {
    if (connected()) {
        mqttDisconnect();
    }
    if (m_mqtt != nullptr) {
        mosquitto_loop_stop(m_mqtt, true);

        mosquitto_destroy(m_mqtt);
        m_mqtt = nullptr;
        mosquitto_lib_cleanup();
    }
}

void MqttHandler::subscribe(const QString& topic)
{
    if (!connected()) {
        return;
    }

    m_topics.emplace_back(topic.toStdString());

    auto result { mosquitto_subscribe(m_mqtt, nullptr, topic.toStdString().c_str(), 1) };

    if (result != MOSQ_ERR_SUCCESS) {
        switch (result) {
        case MOSQ_ERR_INVAL:
            qWarning() << "Could not subscribe to topic '" + topic + "': invalid parameters";
            break;
        case MOSQ_ERR_NOMEM:
            qWarning() << "Could not subscribe to topic '" + topic + "': memory exceeded";
            break;
        case MOSQ_ERR_NO_CONN:
            qWarning() << "Could not subscribe to topic '" + topic + "': Not connected";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            qWarning() << "Could not subscribe to topic '" + topic + "': malformed utf8";
            break;
        default:
            qWarning() << "Could not subscribe to topic '" + topic + "': other reason";
            break;
        }
        return;
    }
    qInfo() << "Subscribed to topic '" + topic + "'.";
}

void MqttHandler::unsubscribe(const QString& topic)
{
    if (!connected()) {
        return;
    }

    auto pos { std::find(std::begin(m_topics), std::end(m_topics), topic.toStdString()) };
    if (pos != std::end(m_topics)) {
        m_topics.erase(pos);
    }

    auto result { mosquitto_unsubscribe(m_mqtt, nullptr, topic.toStdString().c_str()) };

    if (result != MOSQ_ERR_SUCCESS) {
        switch (result) {
        case MOSQ_ERR_INVAL:
            qWarning() << "Could not unsubscribe from topic '" + topic + "': invalid parameters";
            break;
        case MOSQ_ERR_NOMEM:
            qWarning() << "Could not unsubscribe from topic '" + topic + "': memory exceeded";
            break;
        case MOSQ_ERR_NO_CONN:
            qWarning() << "Could not unsubscribe from topic '" + topic + "': Not connected";
            break;
        case MOSQ_ERR_MALFORMED_UTF8:
            qWarning() << "Could not unsubscribe from topic '" + topic + "': malformed utf8";
            break;
        default:
            qWarning() << "Could not unsubscribe from topic '" + topic + "': other reason";
            break;
        }
        return;
    }
    qInfo() << "Unsubscribed from topic '" + topic + "'.";
}

void MqttHandler::sendData(const QString &message){
    if (!publish(m_data_topic, message.toStdString())) {
        qWarning() << "Couldn't publish data";
    }
}

void MqttHandler::sendLog(const QString &message){
    if (!publish(m_log_topic, message.toStdString())) {
        qWarning() << "Couldn't publish log";
    }
}

auto MqttHandler::publish(const std::string& topic, const std::string& content) -> bool {
    if (!connected()) {
        return false;
    }
    auto result { mosquitto_publish(m_mqtt, nullptr, topic.c_str(), static_cast<int>(content.size()), reinterpret_cast<const void*>(content.c_str()), 1, false) };

    if (result == MOSQ_ERR_SUCCESS) {
        return true;
    }
    qWarning() << "Couldn't publish mqtt message: " + QString{strerror(result)};
    return false;
}

void MqttHandler::onRequestConnectionStatus(){
    emit mqttConnectionStatus(m_status == Status::Connected);
}
} // namespace MuonPi
