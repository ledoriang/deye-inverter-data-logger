#include "MqttTopicPublisher.h"

MqttTopicPublisher::MqttTopicPublisher(MqttClientHandler &mqtt, const char *topic, bool retained)
    : mqttClient(mqtt), mqttTopic(topic), retained(retained)
{
}

bool MqttTopicPublisher::publishText(const char *payload) const
{
    return mqttClient.publish(mqttTopic, payload, strlen(payload), retained);
}

bool MqttTopicPublisher::publishJson(const char *payload, size_t payloadLength) const
{
    return mqttClient.publish(mqttTopic, payload, payloadLength, retained);
}

const char *MqttTopicPublisher::topic() const
{
    return mqttTopic;
}
