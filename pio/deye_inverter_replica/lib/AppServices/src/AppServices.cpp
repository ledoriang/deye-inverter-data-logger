#include "AppServices.h"

namespace
{
    constexpr char ONLINE_PAYLOAD[] = "online";
}

AppServices::AppServices(
    WiFiConnectionHandler &wifiService,
    MqttClientHandler &mqttService,
    DeyeInverterReader &inverterReader,
    HaDiscovery &homeAssistant,
    const char *availabilityTopicName,
    uint8_t ledPin,
    bool ledActiveHigh)
    : wifi(wifiService),
      mqtt(mqttService),
      inverter(inverterReader),
      ha(homeAssistant),
      availabilityTopic(availabilityTopicName),
      statusLedPin(ledPin),
      statusLedActiveHigh(ledActiveHigh),
      mqttWasConnected(false)
{
}

bool AppServices::begin()
{
    pinMode(statusLedPin, OUTPUT);
    digitalWrite(statusLedPin, statusLedActiveHigh ? LOW : HIGH);

    const bool wifiReady = wifi.begin();
    const bool mqttReady = mqtt.begin();
    const bool modbusReady = inverter.begin();
    return wifiReady && mqttReady && modbusReady;
}

void AppServices::loop(uint32_t nowMs)
{
    wifi.loop(nowMs);
    mqtt.loop(nowMs, wifi.isConnected());

    const bool mqttConnected = mqtt.isConnected();
    if (mqttConnected && !mqttWasConnected)
    {
        mqtt.publish(availabilityTopic, ONLINE_PAYLOAD, sizeof(ONLINE_PAYLOAD) - 1, true);
        ha.publishAll();
        ha.publishRecent(inverter.telemetry(), nowMs, 120000UL);
    }
    mqttWasConnected = mqttConnected;

    if (inverter.pollIfDue(nowMs) && mqttConnected)
    {
        ha.publishUpdated(inverter.telemetry());
    }

    const uint8_t ledOn = statusLedActiveHigh ? HIGH : LOW;
    const uint8_t ledOff = statusLedActiveHigh ? LOW : HIGH;
    digitalWrite(statusLedPin, mqttConnected ? ledOn : ledOff);
}
