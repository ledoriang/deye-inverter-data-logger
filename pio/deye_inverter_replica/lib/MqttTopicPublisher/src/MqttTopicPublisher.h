#pragma once

#include <Arduino.h>
#include <MqttClientHandler.h>

class MqttTopicPublisher
{
public:
    MqttTopicPublisher(MqttClientHandler &mqtt, const char *topic, bool retained = false);

    bool publishText(const char *payload) const;
    bool publishJson(const char *payload, size_t payloadLength) const;
    const char *topic() const;

private:
    MqttClientHandler &mqttClient;
    const char *mqttTopic;
    bool retained;
};
