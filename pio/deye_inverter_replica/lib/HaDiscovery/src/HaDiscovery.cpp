#include "HaDiscovery.h"

#include <ArduinoJson.h>

namespace
{
    struct SensorDefinition
    {
        DeyeSensor sensor;
        const char *objectId;
        const char *name;
        const char *unit;
        const char *deviceClass;
        const char *stateClass;
        const char *icon;
        uint8_t precision;
    };

    constexpr SensorDefinition SENSOR_DEFINITIONS[DEYE_SENSOR_COUNT] = {
        {DeyeSensor::Pv1Power, "pv1_power", "PV1 Power", "W", "power", "measurement", "mdi:solar-power", 0},
        {DeyeSensor::Pv2Power, "pv2_power", "PV2 Power", "W", "power", "measurement", "mdi:solar-power", 0},
        {DeyeSensor::BatterySoc, "inverter_battery_soc", "Inverter Battery SOC", "%", "battery", "measurement", "mdi:battery", 0},
        {DeyeSensor::BatteryPower, "inverter_battery_power", "Inverter Battery Power", "W", "power", "measurement", "mdi:flash", 0},
        {DeyeSensor::BatteryVoltage, "inverter_battery_voltage", "Inverter Battery Voltage", "V", "voltage", "measurement", "mdi:flash", 1},
        {DeyeSensor::TotalLoadPower, "total_load_power", "Total Load Power", "W", "power", "measurement", "mdi:home-lightning-bolt", 0},
        {DeyeSensor::LoadL1Power, "load_l1_power", "Load L1 Power", "W", "power", "measurement", "mdi:flash", 0},
        {DeyeSensor::LoadL2Power, "load_l2_power", "Load L2 Power", "W", "power", "measurement", "mdi:flash", 0},
        {DeyeSensor::LoadL3Power, "load_l3_power", "Load L3 Power", "W", "power", "measurement", "mdi:flash", 0},
        {DeyeSensor::DayPvEnergy, "day_pv_energy", "Day PV Energy", "kWh", "energy", "total_increasing", "mdi:solar-power", 1},
        {DeyeSensor::DayBatteryCharge, "day_battery_charge", "Day Battery Charge", "kWh", "energy", "total_increasing", "mdi:battery-arrow-up", 1},
        {DeyeSensor::DayBatteryDischarge, "day_battery_discharge", "Day Battery Discharge", "kWh", "energy", "total_increasing", "mdi:battery-arrow-down", 1},
        {DeyeSensor::DayLoadEnergy, "day_load_energy", "Day Load Energy", "kWh", "energy", "total_increasing", "mdi:home-lightning-bolt", 1}};

    bool hasRequiredSettings(const HaDiscoverySettings &settings)
    {
        return settings.discoveryPrefix != nullptr && settings.discoveryPrefix[0] != '\0' &&
            settings.discoveryNodeId != nullptr && settings.discoveryNodeId[0] != '\0' &&
            settings.deviceId != nullptr && settings.deviceId[0] != '\0' &&
            settings.deviceName != nullptr && settings.deviceManufacturer != nullptr &&
            settings.deviceModel != nullptr && settings.stateTopicPrefix != nullptr &&
            settings.availabilityTopic != nullptr && settings.expireAfterSeconds > 0;
    }
}

HaDiscovery::HaDiscovery(
    MqttClientHandler &mqttClient,
    const HaDiscoverySettings &discoverySettings,
    Stream &logOut)
    : mqtt(mqttClient), settings(discoverySettings), log(logOut)
{
}

bool HaDiscovery::publishAll()
{
    if (!hasRequiredSettings(settings))
    {
        log.println("[HA] Invalid discovery settings");
        return false;
    }

    bool allPublished = true;
    for (uint8_t i = 0; i < DEYE_SENSOR_COUNT; ++i)
    {
        allPublished = publishSensorConfig(i) && allPublished;
    }
    return allPublished;
}

bool HaDiscovery::publishUpdated(const DeyeTelemetry &telemetry)
{
    return publishStateMask(telemetry, telemetry.updatedMask);
}

bool HaDiscovery::publishRecent(const DeyeTelemetry &telemetry, uint32_t nowMs, uint32_t maxAgeMs)
{
    uint16_t recentMask = 0;
    for (uint8_t i = 0; i < DEYE_SENSOR_COUNT; ++i)
    {
        const DeyeSensor sensor = SENSOR_DEFINITIONS[i].sensor;
        const uint8_t valueIndex = static_cast<uint8_t>(sensor);
        const uint16_t bit = deyeSensorBit(sensor);
        if ((telemetry.validMask & bit) != 0 &&
            (nowMs - telemetry.updatedAtMs[valueIndex]) <= maxAgeMs)
        {
            recentMask |= bit;
        }
    }
    return publishStateMask(telemetry, recentMask);
}

bool HaDiscovery::publishSensorConfig(uint8_t index)
{
    const SensorDefinition &definition = SENSOR_DEFINITIONS[index];
    char topic[192];
    char stateTopic[192];
    char uniqueId[128];
    snprintf(
        topic,
        sizeof(topic),
        "%s/sensor/%s/%s/config",
        settings.discoveryPrefix,
        settings.discoveryNodeId,
        definition.objectId);
    snprintf(
        stateTopic,
        sizeof(stateTopic),
        "%s/state/%s",
        settings.stateTopicPrefix,
        definition.objectId);
    snprintf(uniqueId, sizeof(uniqueId), "%s_%s", settings.deviceId, definition.objectId);

    JsonDocument document;
    document["name"] = definition.name;
    document["unique_id"] = uniqueId;
    document["state_topic"] = stateTopic;
    document["availability_topic"] = settings.availabilityTopic;
    document["payload_available"] = "online";
    document["payload_not_available"] = "offline";
    document["expire_after"] = settings.expireAfterSeconds;
    document["unit_of_measurement"] = definition.unit;
    document["device_class"] = definition.deviceClass;
    document["state_class"] = definition.stateClass;
    document["icon"] = definition.icon;
    document["suggested_display_precision"] = definition.precision;

    JsonObject device = document["device"].to<JsonObject>();
    device["identifiers"].to<JsonArray>().add(settings.deviceId);
    device["name"] = settings.deviceName;
    device["manufacturer"] = settings.deviceManufacturer;
    device["model"] = settings.deviceModel;

    char payload[768];
    const size_t length = serializeJson(document, payload, sizeof(payload));
    if (length == 0 || length >= sizeof(payload))
    {
        log.print("[HA] Discovery serialization failed for ");
        log.println(definition.objectId);
        return false;
    }

    const bool published = mqtt.publish(topic, payload, length, true);
    log.print("[HA] Discovery ");
    log.print(definition.objectId);
    log.println(published ? " published" : " publish failed");
    return published;
}

bool HaDiscovery::publishState(uint8_t index, float value)
{
    const SensorDefinition &definition = SENSOR_DEFINITIONS[index];
    char topic[192];
    char payload[24];
    snprintf(
        topic,
        sizeof(topic),
        "%s/state/%s",
        settings.stateTopicPrefix,
        definition.objectId);
    const int length = snprintf(payload, sizeof(payload), "%.*f", definition.precision, static_cast<double>(value));
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload))
    {
        return false;
    }
    return mqtt.publish(topic, payload, static_cast<size_t>(length), true);
}

bool HaDiscovery::publishStateMask(const DeyeTelemetry &telemetry, uint16_t mask)
{
    bool allPublished = true;
    for (uint8_t i = 0; i < DEYE_SENSOR_COUNT; ++i)
    {
        const DeyeSensor sensor = SENSOR_DEFINITIONS[i].sensor;
        const uint8_t valueIndex = static_cast<uint8_t>(sensor);
        const uint16_t bit = deyeSensorBit(sensor);
        if ((mask & bit) == 0 || (telemetry.validMask & bit) == 0)
        {
            continue;
        }
        allPublished = publishState(i, telemetry.values[valueIndex]) && allPublished;
    }
    return allPublished;
}
