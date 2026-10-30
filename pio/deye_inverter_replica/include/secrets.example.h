#pragma once

#include <stdint.h>

// Copy this file to secrets.h and fill in the ESP32's Wi-Fi and dedicated MQTT broker account.
// secrets.h is excluded from version control.
// This is the Wi-Fi network used by the ESP32 for LAN/Home Assistant access;
// the inverter is reached separately over UART/RS-485.
namespace Secrets
{
    constexpr char WIFI_SSID[] = "";
    constexpr char WIFI_PASSWORD[] = "";

    // Host and credentials for the existing MQTT broker.
    constexpr char MQTT_BROKER[] = "";
    constexpr uint16_t MQTT_PORT = 1883;
    constexpr char MQTT_CLIENT_ID[] = "deye-sun-12k-esp32c6";
    constexpr char MQTT_USER[] = "";
    constexpr char MQTT_PASSWORD[] = "";
}
