#pragma once

#include <Arduino.h>
#include <NetworkAttemptTracker.h>
#include <WiFi.h>

struct WiFiConnectionSettings
{
    // String values are borrowed; keep them alive for the handler's lifetime.
    const char *ssid = nullptr;
    const char *password = nullptr;
    const char *hostname = nullptr;
    uint8_t maxRetries = 5;
    uint32_t connectTimeoutMs = 25000;
    uint32_t initialRetryIntervalMs = 2000;
    uint32_t maxRetryIntervalMs = 30000;
    uint32_t cooldownMs = 30000;
    bool scanForNetworksOnStart = true;
    wifi_scan_method_t scanMethod = WIFI_ALL_CHANNEL_SCAN;
    wifi_sort_method_t sortMethod = WIFI_CONNECT_AP_BY_SIGNAL;
};

class WiFiConnectionHandler
{
public:
    WiFiConnectionHandler(
        const WiFiConnectionSettings &settings,
        NetworkAttemptTracker &attemptTracker,
        Stream &logOut);

    bool begin();
    void loop(uint32_t nowMs);
    bool isConnected() const;
    uint32_t totalConnectionAttempts() const;

private:
    void logScanResults(int16_t networkCount);

    WiFiConnectionSettings settings;
    NetworkAttemptTracker &attemptTracker;
    uint32_t currentRetryInterval;
    Stream &log;
    uint32_t attemptsSinceLastConnection;

    uint8_t retryCount;
    uint32_t lastAttemptMs;
    uint32_t cooldownStartedMs;
    bool cooldownActive;
    bool attemptInProgress;
    bool scanInProgress;
    bool wasConnected;
    bool configurationValid;
};
