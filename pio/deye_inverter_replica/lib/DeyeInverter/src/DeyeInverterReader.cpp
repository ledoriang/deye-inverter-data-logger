#include "DeyeInverterReader.h"

namespace
{
    const char *const SENSOR_NAMES[DEYE_SENSOR_COUNT] = {
        "PV1 Power",
        "PV2 Power",
        "Inverter Battery SOC",
        "Inverter Battery Power",
        "Inverter Battery Voltage",
        "Total Load Power",
        "Load L1 Power",
        "Load L2 Power",
        "Load L3 Power",
        "Day PV Energy",
        "Day Battery Charge",
        "Day Battery Discharge",
        "Day Load Energy"};
}

DeyeInverterReader::DeyeInverterReader(
    ModbusRtuClient &modbusClient,
    uint32_t intervalMs,
    uint32_t socIntervalMs,
    Stream &logOut)
    : modbus(modbusClient),
      pollIntervalMs(intervalMs),
      batterySocIntervalMs(socIntervalMs),
      log(logOut),
      current{},
      lastPollMs(0),
      lastBatterySocPollMs(0),
      initialized(false),
      hasPolled(false),
      hasPolledBatterySoc(false)
{
}

bool DeyeInverterReader::begin()
{
    initialized = modbus.begin();
    if (initialized)
    {
        log.println("[Deye] Register reader ready");
    }
    return initialized;
}

bool DeyeInverterReader::pollIfDue(uint32_t nowMs)
{
    if (!initialized || (hasPolled && (nowMs - lastPollMs) < pollIntervalMs))
    {
        return false;
    }

    hasPolled = true;
    lastPollMs = nowMs;
    current.updatedMask = 0;
    bool cycleOk = true;

    if (!hasPolledBatterySoc || (nowMs - lastBatterySocPollMs) >= batterySocIntervalMs)
    {
        if (readSensor(DeyeSensor::BatterySoc, 588, false, 1.0f, nowMs))
        {
            lastBatterySocPollMs = nowMs;
            hasPolledBatterySoc = true;
        }
        else
        {
            cycleOk = false;
        }
    }

    cycleOk = readSensor(DeyeSensor::Pv1Power, 672, false, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::Pv2Power, 673, false, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::BatteryPower, 590, true, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::BatteryVoltage, 587, false, 0.1f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::TotalLoadPower, 604, true, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::LoadL1Power, 601, true, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::LoadL2Power, 602, true, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::LoadL3Power, 603, true, 1.0f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::DayPvEnergy, 676, false, 0.1f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::DayBatteryCharge, 592, false, 0.1f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::DayBatteryDischarge, 593, false, 0.1f, nowMs) && cycleOk;
    cycleOk = readSensor(DeyeSensor::DayLoadEnergy, 607, false, 0.1f, nowMs) && cycleOk;

    if (!cycleOk)
    {
        log.println("[Deye] Poll cycle had one or more failed register reads");
    }
    return true;
}

bool DeyeInverterReader::readSensor(
    DeyeSensor sensor,
    uint16_t registerAddress,
    bool signedValue,
    float scale,
    uint32_t nowMs)
{
    uint16_t rawValue = 0;
    uint8_t exceptionCode = 0;
    const ModbusResult result = modbus.readHoldingRegisters(registerAddress, 1, &rawValue, 1, &exceptionCode);
    if (result != ModbusResult::Ok)
    {
        log.print("[Deye] Read failed for ");
        log.print(deyeSensorName(sensor));
        log.print(" at register ");
        log.print(registerAddress);
        log.print(" (result ");
        log.print(static_cast<uint8_t>(result));
        log.print(")");
        if (result == ModbusResult::ExceptionResponse)
        {
            log.print(", exception ");
            log.print(exceptionCode);
        }
        log.println();
        return false;
    }

    const float decoded = signedValue
        ? static_cast<float>(static_cast<int16_t>(rawValue))
        : static_cast<float>(rawValue);
    const uint8_t index = static_cast<uint8_t>(sensor);
    current.values[index] = decoded * scale;
    current.validMask |= deyeSensorBit(sensor);
    current.updatedMask |= deyeSensorBit(sensor);
    current.updatedAtMs[index] = nowMs;

    log.print("[Deye] ");
    log.print(deyeSensorName(sensor));
    log.print(" raw=");
    log.print(rawValue);
    log.print(" value=");
    const uint8_t precision = sensor == DeyeSensor::BatteryVoltage ||
            sensor == DeyeSensor::DayPvEnergy ||
            sensor == DeyeSensor::DayBatteryCharge ||
            sensor == DeyeSensor::DayBatteryDischarge ||
            sensor == DeyeSensor::DayLoadEnergy
        ? 1
        : 0;
    log.println(current.values[index], precision);
    return true;
}

const DeyeTelemetry &DeyeInverterReader::telemetry() const
{
    return current;
}

const char *deyeSensorName(DeyeSensor sensor)
{
    const uint8_t index = static_cast<uint8_t>(sensor);
    return index < DEYE_SENSOR_COUNT ? SENSOR_NAMES[index] : "Unknown Deye sensor";
}

uint16_t deyeSensorBit(DeyeSensor sensor)
{
    return static_cast<uint16_t>(1U << static_cast<uint8_t>(sensor));
}
