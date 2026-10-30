#pragma once

#include <Arduino.h>
#include <DeyeInverterReader.h>
#include <HaDiscovery.h>
#include <MqttClientHandler.h>
#include <WiFiConnectionHandler.h>

class AppServices
{
public:
    AppServices(
        WiFiConnectionHandler &wifiService,
        MqttClientHandler &mqttService,
        DeyeInverterReader &inverterReader,
        HaDiscovery &homeAssistant,
        const char *availabilityTopic,
        uint8_t statusLedPin,
        bool statusLedActiveHigh = true);

    bool begin();
    void loop(uint32_t nowMs);

private:
    WiFiConnectionHandler &wifi;
    MqttClientHandler &mqtt;
    DeyeInverterReader &inverter;
    HaDiscovery &ha;
    const char *availabilityTopic;
    uint8_t statusLedPin;
    bool statusLedActiveHigh;
    bool mqttWasConnected;
};
