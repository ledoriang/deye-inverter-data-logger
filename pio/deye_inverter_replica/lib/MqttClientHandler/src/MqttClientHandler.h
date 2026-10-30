#pragma once

#include <Arduino.h>
#include <Client.h>
#include <NetworkAttemptTracker.h>
#include <PubSubClient.h>

struct MqttClientSettings
{
    // String values are borrowed; keep them alive for the handler's lifetime.
    const char *broker = nullptr;
    uint16_t port = 1883;
    const char *clientId = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;

    uint8_t maxRetries = 5;
    uint32_t initialRetryIntervalMs = 2000;
    uint32_t maxRetryIntervalMs = 30000;
    uint32_t cooldownMs = 30000;

    const char *willTopic = nullptr;
    const char *willPayload = nullptr;
    uint8_t willQos = 0;
    bool willRetained = false;

    uint16_t bufferSize = 256;
    uint16_t socketTimeoutSeconds = 5;
    uint16_t keepAliveSeconds = 15;
};

class MqttClientHandler
{
public:
    MqttClientHandler(
        Client &transport,
        const MqttClientSettings &settings,
        NetworkAttemptTracker &attemptTracker,
        Stream &logOut);

    bool begin();
    void loop(uint32_t nowMs, bool networkReady);
    bool isConnected();
    bool publish(const char *topic, const char *payload, size_t payloadLength, bool retained = false);
    uint32_t totalConnectionAttempts() const;

private:
    void logConnectionSuccess();
    void logConnectionLost(int state);

    MqttClientSettings settings;
    NetworkAttemptTracker &attemptTracker;
    uint32_t currentRetryInterval;
    Stream &log;
    uint32_t attemptsSinceLastConnection;

    // The supplied Client transport must outlive this handler.
    PubSubClient client;

    uint8_t retryCount;
    uint32_t lastAttemptMs;
    uint32_t cooldownStartedMs;
    bool cooldownActive;
    bool wasConnected;
    bool configurationValid;
};
