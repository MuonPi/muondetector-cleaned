#ifndef MQTTHANDLER_H
#define MQTTHANDLER_H

#include <QObject>
#include <async_client.h>
#include <connect_options.h>

class callback : public virtual mqtt::callback{
public:
    void connection_lost(const std::string& cause) override;
    void delivery_complete(mqtt::delivery_token_ptr tok) override;
};

class action_listener : public virtual mqtt::iaction_listener{
protected:
    void on_failure(const mqtt::token& tok) override;
    void on_success(const mqtt::token& tok) override;
};

class delivery_action_listener : public action_listener{
public:
    delivery_action_listener() : done_(false) {}
    void on_failure(const mqtt::token& tok) override;
    void on_success(const mqtt::token& tok) override;
    bool is_done() const { return done_; }
private:
    std::atomic<bool> done_;
};

class MqttHandler : public QObject
{
    Q_OBJECT

    signals:
        void mqttConnectionStatus(bool connected);

    public slots:
        void start(QString username, QString password, QString station_ID);
        void sendData(const QString &message);
        void sendLog(const QString &message);
        void onRequestConnectionStatus();

    public:
        MqttHandler();
        //using QObject::QObject;
        void mqttConnect();
        void mqttDisconnect();

    private:
        const int qos = 1;
        mqtt::async_client *mqttClient;
        mqtt::topic *data_topic;
        mqtt::topic *log_topic;
        mqtt::connect_options *conopts;
        mqtt::message *willmsg;
        mqtt::will_options *will;
        bool _mqttConnectionStatus = false;
        QString mqttAddress = "116.202.96.181:1883";
        QString stationID;
        QString username;
        QString password;
};

#endif // MQTTHANDLER_H