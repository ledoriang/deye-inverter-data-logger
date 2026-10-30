#pragma once

#include <ProjectConfig.h>
#include <HaDiscovery.h>
#include <MqttClientHandler.h>
#include <ModbusRtuClient.h>
#include <WiFiConnectionHandler.h>

namespace ProjectSettings
{
    inline WiFiConnectionSettings wifi()
    {
        WiFiConnectionSettings settings;
        settings.ssid = ProjectConfig::LanWifiSsid;
        settings.password = ProjectConfig::LanWifiPassword;
        settings.hostname = ProjectConfig::Hostname;
        return settings;
    }

    inline MqttClientSettings mqtt()
    {
        MqttClientSettings settings;
        settings.broker = ProjectConfig::MqttBrokerHost;
        settings.port = ProjectConfig::MqttBrokerPort;
        settings.clientId = ProjectConfig::MqttClientId;
        settings.username = ProjectConfig::MqttBrokerUsername[0] == '\0' ? nullptr : ProjectConfig::MqttBrokerUsername;
        settings.password = ProjectConfig::MqttBrokerPassword[0] == '\0' ? nullptr : ProjectConfig::MqttBrokerPassword;
        settings.willTopic = ProjectConfig::AvailabilityTopic;
        settings.willPayload = "offline";
        settings.willQos = 0;
        settings.willRetained = true;
        settings.bufferSize = ProjectConfig::MqttBufferSize;
        return settings;
    }

    inline ModbusRtuSettings modbus()
    {
        ModbusRtuSettings settings;
        settings.slaveAddress = ProjectConfig::ModbusSlaveAddress;
        settings.txPin = ProjectConfig::ModbusTxPin;
        settings.rxPin = ProjectConfig::ModbusRxPin;
        settings.directionPin = ProjectConfig::ModbusDirectionPin;
        settings.baudRate = ProjectConfig::ModbusBaud;
        settings.responseTimeoutMs = ProjectConfig::ModbusResponseTimeoutMs;
        settings.sendWaitMs = ProjectConfig::ModbusSendWaitMs;
        return settings;
    }

    inline HaDiscoverySettings homeAssistant()
    {
        HaDiscoverySettings settings;
        settings.discoveryPrefix = ProjectConfig::DiscoveryPrefix;
        settings.discoveryNodeId = ProjectConfig::DeviceId;
        settings.deviceId = ProjectConfig::DeviceId;
        settings.deviceName = ProjectConfig::DeviceName;
        settings.deviceManufacturer = ProjectConfig::DeviceManufacturer;
        settings.deviceModel = ProjectConfig::DeviceModel;
        settings.stateTopicPrefix = ProjectConfig::StateTopicPrefix;
        settings.availabilityTopic = ProjectConfig::AvailabilityTopic;
        settings.expireAfterSeconds = 180;
        return settings;
    }
}
