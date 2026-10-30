#pragma once

#include <Arduino.h>

struct ModbusRtuSettings
{
    uint8_t slaveAddress = 1;
    int8_t txPin = -1;
    int8_t rxPin = -1;
    uint8_t directionPin = 4;
    uint32_t baudRate = 9600;
    uint16_t responseTimeoutMs = 500;
    uint16_t sendWaitMs = 100;
    bool transmitLevel = true;
};

enum class ModbusResult : uint8_t
{
    Ok,
    NotInitialized,
    InvalidRequest,
    Timeout,
    InvalidResponse,
    CrcError,
    ExceptionResponse
};

class ModbusRtuClient
{
public:
    ModbusRtuClient(HardwareSerial &serial, const ModbusRtuSettings &settings, Stream &logOut);

    bool begin();
    ModbusResult readHoldingRegisters(
        uint16_t firstRegister,
        uint16_t registerCount,
        uint16_t *destination,
        size_t destinationCapacity,
        uint8_t *exceptionCode = nullptr);

private:
    static uint16_t crc16(const uint8_t *data, size_t length);
    bool readByte(uint8_t &value, uint32_t startedAtMs);
    void waitBetweenRequests();

    HardwareSerial &serial;
    ModbusRtuSettings settings;
    Stream &log;
    bool initialized;
    bool hasTransacted;
    uint32_t lastTransactionMs;
};
