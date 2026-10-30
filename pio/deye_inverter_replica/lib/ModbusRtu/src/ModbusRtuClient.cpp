#include "ModbusRtuClient.h"

namespace
{
    constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;
    constexpr uint8_t MAX_READ_REGISTERS = 125;
    constexpr size_t MAX_RESPONSE_BYTES = 256;
}

ModbusRtuClient::ModbusRtuClient(
    HardwareSerial &serialPort,
    const ModbusRtuSettings &clientSettings,
    Stream &logOut)
    : serial(serialPort),
      settings(clientSettings),
      log(logOut),
      initialized(false),
      hasTransacted(false),
      lastTransactionMs(0)
{
}

bool ModbusRtuClient::begin()
{
    if (settings.txPin < 0 || settings.rxPin < 0 || settings.baudRate == 0 ||
        settings.responseTimeoutMs == 0 || settings.slaveAddress == 0)
    {
        log.println("[Modbus] Invalid RTU settings");
        initialized = false;
        return false;
    }

    pinMode(settings.directionPin, OUTPUT);
    digitalWrite(settings.directionPin, !settings.transmitLevel);
    serial.begin(settings.baudRate, SERIAL_8E1, settings.rxPin, settings.txPin);
    initialized = true;

    log.print("[Modbus] UART ready: ");
    log.print(settings.baudRate);
    log.println(" baud, 8E1");
    return true;
}

ModbusResult ModbusRtuClient::readHoldingRegisters(
    uint16_t firstRegister,
    uint16_t registerCount,
    uint16_t *destination,
    size_t destinationCapacity,
    uint8_t *exceptionCode)
{
    if (!initialized)
    {
        return ModbusResult::NotInitialized;
    }
    if (destination == nullptr || registerCount == 0 || registerCount > MAX_READ_REGISTERS ||
        destinationCapacity < registerCount ||
        static_cast<uint32_t>(firstRegister) + registerCount > 65536UL)
    {
        return ModbusResult::InvalidRequest;
    }

    waitBetweenRequests();
    while (serial.available() > 0)
    {
        serial.read();
    }

    uint8_t request[8] = {
        settings.slaveAddress,
        READ_HOLDING_REGISTERS,
        static_cast<uint8_t>(firstRegister >> 8),
        static_cast<uint8_t>(firstRegister & 0xFF),
        static_cast<uint8_t>(registerCount >> 8),
        static_cast<uint8_t>(registerCount & 0xFF),
        0,
        0};
    const uint16_t requestCrc = crc16(request, 6);
    request[6] = static_cast<uint8_t>(requestCrc & 0xFF);
    request[7] = static_cast<uint8_t>(requestCrc >> 8);

    digitalWrite(settings.directionPin, settings.transmitLevel);
    serial.write(request, sizeof(request));
    serial.flush();
    digitalWrite(settings.directionPin, !settings.transmitLevel);

    const uint32_t startedAtMs = millis();
    uint8_t response[MAX_RESPONSE_BYTES];
    for (size_t i = 0; i < 3; ++i)
    {
        if (!readByte(response[i], startedAtMs))
        {
            lastTransactionMs = millis();
            hasTransacted = true;
            return ModbusResult::Timeout;
        }
    }

    if (response[0] != settings.slaveAddress)
    {
        lastTransactionMs = millis();
        hasTransacted = true;
        return ModbusResult::InvalidResponse;
    }

    const bool exception = response[1] == static_cast<uint8_t>(READ_HOLDING_REGISTERS | 0x80);
    if (!exception && response[1] != READ_HOLDING_REGISTERS)
    {
        lastTransactionMs = millis();
        hasTransacted = true;
        return ModbusResult::InvalidResponse;
    }

    const size_t remainingBytes = exception ? 2 : static_cast<size_t>(response[2]) + 2;
    const size_t responseLength = 3 + remainingBytes;
    if (responseLength > sizeof(response) || (!exception && response[2] != registerCount * 2))
    {
        lastTransactionMs = millis();
        hasTransacted = true;
        return ModbusResult::InvalidResponse;
    }

    for (size_t i = 3; i < responseLength; ++i)
    {
        if (!readByte(response[i], startedAtMs))
        {
            lastTransactionMs = millis();
            hasTransacted = true;
            return ModbusResult::Timeout;
        }
    }

    lastTransactionMs = millis();
    hasTransacted = true;

    const uint16_t receivedCrc = static_cast<uint16_t>(response[responseLength - 2]) |
        (static_cast<uint16_t>(response[responseLength - 1]) << 8);
    if (crc16(response, responseLength - 2) != receivedCrc)
    {
        return ModbusResult::CrcError;
    }

    if (exception)
    {
        if (exceptionCode != nullptr)
        {
            *exceptionCode = response[2];
        }
        return ModbusResult::ExceptionResponse;
    }

    for (uint16_t i = 0; i < registerCount; ++i)
    {
        const size_t dataIndex = 3 + static_cast<size_t>(i) * 2;
        destination[i] = (static_cast<uint16_t>(response[dataIndex]) << 8) |
            static_cast<uint16_t>(response[dataIndex + 1]);
    }
    return ModbusResult::Ok;
}

uint16_t ModbusRtuClient::crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 1) != 0 ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

bool ModbusRtuClient::readByte(uint8_t &value, uint32_t startedAtMs)
{
    while ((millis() - startedAtMs) < settings.responseTimeoutMs)
    {
        if (serial.available() > 0)
        {
            const int byteValue = serial.read();
            if (byteValue >= 0)
            {
                value = static_cast<uint8_t>(byteValue);
                return true;
            }
        }
        delay(1);
    }
    return false;
}

void ModbusRtuClient::waitBetweenRequests()
{
    if (!hasTransacted || settings.sendWaitMs == 0)
    {
        return;
    }

    const uint32_t elapsed = millis() - lastTransactionMs;
    if (elapsed < settings.sendWaitMs)
    {
        delay(settings.sendWaitMs - elapsed);
    }
}
