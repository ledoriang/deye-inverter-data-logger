#pragma once

#include <Arduino.h>
#include <ModbusRtuClient.h>

enum class DeyeSensor : uint8_t
{
    Pv1Power,
    Pv2Power,
    BatterySoc,
    BatteryPower,
    BatteryVoltage,
    TotalLoadPower,
    LoadL1Power,
    LoadL2Power,
    LoadL3Power,
    DayPvEnergy,
    DayBatteryCharge,
    DayBatteryDischarge,
    DayLoadEnergy,
    Count
};

static constexpr uint8_t DEYE_SENSOR_COUNT = static_cast<uint8_t>(DeyeSensor::Count);

struct DeyeTelemetry
{
    float values[DEYE_SENSOR_COUNT] = {};
    uint16_t validMask = 0;
    uint16_t updatedMask = 0;
    uint32_t updatedAtMs[DEYE_SENSOR_COUNT] = {};
};

class DeyeInverterReader
{
public:
    DeyeInverterReader(
        ModbusRtuClient &modbus,
        uint32_t pollIntervalMs,
        uint32_t batterySocIntervalMs,
        Stream &logOut);

    bool begin();
    bool pollIfDue(uint32_t nowMs);
    const DeyeTelemetry &telemetry() const;

private:
    bool readSensor(
        DeyeSensor sensor,
        uint16_t registerAddress,
        bool signedValue,
        float scale,
        uint32_t nowMs);

    ModbusRtuClient &modbus;
    uint32_t pollIntervalMs;
    uint32_t batterySocIntervalMs;
    Stream &log;
    DeyeTelemetry current;
    uint32_t lastPollMs;
    uint32_t lastBatterySocPollMs;
    bool initialized;
    bool hasPolled;
    bool hasPolledBatterySoc;
};

const char *deyeSensorName(DeyeSensor sensor);
uint16_t deyeSensorBit(DeyeSensor sensor);
