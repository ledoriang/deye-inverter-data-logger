#include <Arduino.h>
#include <WiFi.h>

#include <AppServices.h>
#include <DeyeInverterReader.h>
#include <HaDiscovery.h>
#include <MqttClientHandler.h>
#include <ModbusRtuClient.h>
#include <NetworkAttemptTracker.h>
#include <ProjectSettings.h>
#include <WiFiConnectionHandler.h>

namespace
{
    HardwareSerial modbusSerial(1);
    NetworkAttemptTracker networkAttempts;
    WiFiClient mqttTransport;

    const WiFiConnectionSettings wifiSettings = ProjectSettings::wifi();
    const MqttClientSettings mqttSettings = ProjectSettings::mqtt();
    const ModbusRtuSettings modbusSettings = ProjectSettings::modbus();
    const HaDiscoverySettings haSettings = ProjectSettings::homeAssistant();

    WiFiConnectionHandler wifiService(wifiSettings, networkAttempts, Serial);
    MqttClientHandler mqttService(mqttTransport, mqttSettings, networkAttempts, Serial);
    ModbusRtuClient modbusClient(modbusSerial, modbusSettings, Serial);
    DeyeInverterReader inverterReader(modbusClient, ProjectConfig::ModbusPollIntervalMs, 90000UL, Serial);
    HaDiscovery homeAssistant(mqttService, haSettings, Serial);
    AppServices appServices(
        wifiService,
        mqttService,
        inverterReader,
        homeAssistant,
        ProjectConfig::AvailabilityTopic,
        ProjectConfig::StatusLedPin);
}

void setup()
{
    Serial.begin(115200);
    appServices.begin();
}

void loop()
{
    appServices.loop(millis());
    delay(1);
}
