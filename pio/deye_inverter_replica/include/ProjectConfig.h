#pragma once

#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// These credentials connect the ESP32 to the LAN and MQTT broker. The inverter
// connection is separate and uses the UART/RS-485 settings below.
namespace ProjectConfig
{
    static constexpr const char *LanWifiSsid = Secrets::WIFI_SSID;
    static constexpr const char *LanWifiPassword = Secrets::WIFI_PASSWORD;
    static constexpr const char *MqttBrokerHost = Secrets::MQTT_BROKER;
    static constexpr uint16_t MqttBrokerPort = Secrets::MQTT_PORT;
    static constexpr const char *MqttBrokerUsername = Secrets::MQTT_USER;
    static constexpr const char *MqttBrokerPassword = Secrets::MQTT_PASSWORD;

    static constexpr char Hostname[] = "deye-sun-12k-esp32c6";
    static constexpr const char *MqttClientId = Secrets::MQTT_CLIENT_ID;
    static constexpr char DeviceId[] = "deye_sun_12k_esp32c6";
    static constexpr char DeviceName[] = "Deye SUN-12K";
    static constexpr char DeviceModel[] = "SUN-12K";
    static constexpr char DeviceManufacturer[] = "Deye";

    static constexpr char StateTopicPrefix[] = "deye/sun12k";
    static constexpr char AvailabilityTopic[] = "deye/sun12k/availability";
    static constexpr char DiscoveryPrefix[] = "homeassistant";

    static constexpr uint8_t StatusLedPin = 15;
    static constexpr uint8_t ModbusDirectionPin = 4;
    static constexpr int8_t ModbusTxPin = 16;
    static constexpr int8_t ModbusRxPin = 17;
    static constexpr uint32_t ModbusBaud = 9600;
    static constexpr uint8_t ModbusSlaveAddress = 0x01;
    static constexpr uint16_t ModbusPollIntervalMs = 30000;
    static constexpr uint16_t ModbusSendWaitMs = 100;
    static constexpr uint16_t ModbusResponseTimeoutMs = 500;
    static constexpr uint16_t MqttBufferSize = 1024;
}
