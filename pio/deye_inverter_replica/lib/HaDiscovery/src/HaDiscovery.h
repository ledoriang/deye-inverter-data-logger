#pragma once

#include <Arduino.h>
#include <DeyeInverterReader.h>
#include <MqttClientHandler.h>

struct HaDiscoverySettings
{
    // All string values are borrowed and must outlive this publisher.
    const char *discoveryPrefix = nullptr;
    const char *discoveryNodeId = nullptr;
    const char *deviceId = nullptr;
    const char *deviceName = nullptr;
    const char *deviceManufacturer = nullptr;
    const char *deviceModel = nullptr;
    const char *stateTopicPrefix = nullptr;
    const char *availabilityTopic = nullptr;
    uint32_t expireAfterSeconds = 180;
};

class HaDiscovery
{
public:
    HaDiscovery(MqttClientHandler &mqtt, const HaDiscoverySettings &settings, Stream &logOut);

    bool publishAll();
    bool publishUpdated(const DeyeTelemetry &telemetry);
    bool publishRecent(const DeyeTelemetry &telemetry, uint32_t nowMs, uint32_t maxAgeMs);

private:
    bool publishSensorConfig(uint8_t index);
    bool publishState(uint8_t index, float value);
    bool publishStateMask(const DeyeTelemetry &telemetry, uint16_t mask);

    MqttClientHandler &mqtt;
    HaDiscoverySettings settings;
    Stream &log;
};
