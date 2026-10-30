#include "WiFiConnectionHandler.h"

namespace
{
    const char *wifiStatusName(wl_status_t status)
    {
        switch (status)
        {
        case WL_IDLE_STATUS:
            return "idle";
        case WL_NO_SSID_AVAIL:
            return "SSID not found";
        case WL_SCAN_COMPLETED:
            return "scan completed";
        case WL_CONNECTED:
            return "connected";
        case WL_CONNECT_FAILED:
            return "connect failed";
        case WL_CONNECTION_LOST:
            return "connection lost";
        case WL_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
        }
    }

    void logNetworkDetails(Stream &log, const char *configuredHostname)
    {
        const char *hostname = WiFi.getHostname();

        log.println("[WiFi] Connected");
        log.print("[WiFi]   IP Address: ");
        log.print(WiFi.localIP());
        log.println();

        log.print("[WiFi]   SSID: '");
        log.print(WiFi.SSID());
        log.println("'");

        log.print("[WiFi]   BSSID: ");
        log.println(WiFi.BSSIDstr());

        log.print("[WiFi]   Hostname: '");
        if (hostname != nullptr && hostname[0] != '\0')
        {
            log.print(hostname);
        }
        else if (configuredHostname != nullptr)
        {
            log.print(configuredHostname);
        }
        else
        {
            log.print("(default)");
        }
        log.println("'");

        log.print("[WiFi]   Signal strength: ");
        log.print(WiFi.RSSI());
        log.println(" dBm");

        log.print("[WiFi]   Channel: ");
        log.println(WiFi.channel());

        log.print("[WiFi]   Subnet: ");
        log.println(WiFi.subnetMask());

        log.print("[WiFi]   Gateway: ");
        log.println(WiFi.gatewayIP());

        log.print("[WiFi]   DNS1: ");
        log.println(WiFi.dnsIP(0));

        log.print("[WiFi]   DNS2: ");
        log.println(WiFi.dnsIP(1));

        log.print("[WiFi]   Device MAC: ");
        log.println(WiFi.macAddress());
    }
}

WiFiConnectionHandler::WiFiConnectionHandler(
    const WiFiConnectionSettings &connectionSettings,
    NetworkAttemptTracker &connectionAttemptTracker,
    Stream &logOut)
    : settings(connectionSettings),
      attemptTracker(connectionAttemptTracker),
      currentRetryInterval(connectionSettings.initialRetryIntervalMs),
      log(logOut),
      attemptsSinceLastConnection(0),
      retryCount(0),
      lastAttemptMs(0),
      cooldownStartedMs(0),
      cooldownActive(false),
      attemptInProgress(false),
      scanInProgress(false),
      wasConnected(false),
      configurationValid(false)
{
}

bool WiFiConnectionHandler::begin()
{
    retryCount = 0;
    currentRetryInterval = settings.initialRetryIntervalMs;
    cooldownStartedMs = 0;
    cooldownActive = false;
    attemptInProgress = false;
    scanInProgress = false;
    wasConnected = false;

    if (settings.ssid == nullptr || settings.ssid[0] == '\0' ||
        settings.maxRetries == 0 || settings.connectTimeoutMs == 0 ||
        settings.maxRetryIntervalMs < settings.initialRetryIntervalMs)
    {
        log.println("[WiFi] Invalid connection settings; station not started");
        configurationValid = false;
        return false;
    }

    if (settings.hostname != nullptr && settings.hostname[0] != '\0')
    {
        const bool hostnameSet = WiFi.setHostname(settings.hostname);
        log.print("[WiFi] DHCP hostname ");
        log.print(settings.hostname);
        log.println(hostnameSet ? " configured" : " FAILED to configure");
    }

    configurationValid = WiFi.mode(WIFI_STA);
    if (!configurationValid)
    {
        log.println("[WiFi] Failed to start station mode");
        return false;
    }

    WiFi.setScanMethod(settings.scanMethod);
    WiFi.setSortMethod(settings.sortMethod);
    log.print("[WiFi] AP selection scan=");
    log.print(settings.scanMethod == WIFI_ALL_CHANNEL_SCAN ? "all channels" : "fast");
    log.print(", sort=");
    log.println(settings.sortMethod == WIFI_CONNECT_AP_BY_SIGNAL ? "strongest signal" : "security");

    if (settings.scanForNetworksOnStart)
    {
        log.println("[WiFi] Scanning for nearby networks...");
        const int16_t scanResult = WiFi.scanNetworks(true, true);
        if (scanResult == WIFI_SCAN_RUNNING)
        {
            scanInProgress = true;
        }
        else if (scanResult >= 0)
        {
            logScanResults(scanResult);
            WiFi.scanDelete();
        }
        else
        {
            log.print("[WiFi] Network scan could not start, result=");
            log.println(scanResult);
        }
    }
    return configurationValid;
}

void WiFiConnectionHandler::logScanResults(int16_t networkCount)
{
    log.print("[WiFi] Scan found ");
    log.print(networkCount);
    log.println(" network(s):");

    for (int16_t index = 0; index < networkCount; ++index)
    {
        const String ssid = WiFi.SSID(index);
        const int32_t rssi = WiFi.RSSI(index);
        const int32_t channel = WiFi.channel(index);
        const int32_t authMode = static_cast<int32_t>(WiFi.encryptionType(index));
        const bool isConfiguredSsid = settings.ssid != nullptr && ssid.equals(settings.ssid);

        log.print("  ");
        log.print(index + 1);
        log.print(": SSID=\"");
        log.print(ssid);
        log.print("\" RSSI=");
        log.print(rssi);
        log.print(" dBm channel=");
        log.print(channel);
        log.print(" auth=");
        log.print(authMode);
        log.print(" BSSID=");
        log.print(WiFi.BSSIDstr(index));
        if (isConfiguredSsid)
        {
            log.print(" [configured SSID]");
        }
        log.println();
    }
}

void WiFiConnectionHandler::loop(uint32_t nowMs)
{
    if (!configurationValid)
    {
        return;
    }

    if (scanInProgress)
    {
        const int16_t scanResult = WiFi.scanComplete();
        if (scanResult == WIFI_SCAN_RUNNING)
        {
            return;
        }

        scanInProgress = false;
        if (scanResult >= 0)
        {
            logScanResults(scanResult);
        }
        else
        {
            log.print("[WiFi] Network scan failed, result=");
            log.println(scanResult);
        }
        WiFi.scanDelete();
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected)
    {
        if (!wasConnected)
        {
            logNetworkDetails(log, settings.hostname);
            log.print("[WiFi] Attempts for this connection: ");
            log.print(attemptsSinceLastConnection);
            log.print("; totals since boot (WiFi/MQTT/all): ");
            log.print(attemptTracker.wifiAttempts());
            log.print("/");
            log.print(attemptTracker.mqttAttempts());
            log.print("/");
            log.println(attemptTracker.totalAttempts());
            attemptsSinceLastConnection = 0;
        }

        attemptInProgress = false;
        wasConnected = true;
        retryCount = 0;
        currentRetryInterval = settings.initialRetryIntervalMs;
        cooldownActive = false;
        return;
    }

    if (wasConnected)
    {
        log.print("Wi-Fi connection lost, status=");
        log.print(wifiStatusName(WiFi.status()));
        log.print(" (");
        log.print(static_cast<int>(WiFi.status()));
        log.println(")");
        wasConnected = false;
    }

    if (attemptInProgress)
    {
        if ((nowMs - lastAttemptMs) < settings.connectTimeoutMs)
        {
            return;
        }

        const wl_status_t status = WiFi.status();
        log.print("Wi-Fi connect timed out after ");
        log.print(settings.connectTimeoutMs);
        log.print(" ms, status=");
        log.print(wifiStatusName(status));
        log.print(" (");
        log.print(static_cast<int>(status));
        log.println(")");

        // Stop the old association attempt before asking the ESP32 to configure
        // the station again. Calling WiFi.begin() while it is connecting fails
        // with ESP_ERR_WIFI_STATE.
        WiFi.disconnect(false, false);
        attemptInProgress = false;

        if (retryCount >= settings.maxRetries)
        {
            cooldownStartedMs = nowMs;
            cooldownActive = settings.cooldownMs > 0;
            retryCount = 0;
            currentRetryInterval = settings.initialRetryIntervalMs;

            log.print("Wi-Fi retries exhausted, backing off for ");
            log.print(settings.cooldownMs);
            log.println(" ms");
            return;
        }

        if (currentRetryInterval < settings.maxRetryIntervalMs)
        {
            currentRetryInterval = currentRetryInterval > settings.maxRetryIntervalMs / 2
                ? settings.maxRetryIntervalMs
                : currentRetryInterval * 2;
        }
        lastAttemptMs = nowMs;
        return;
    }

    if (cooldownActive)
    {
        if ((nowMs - cooldownStartedMs) < settings.cooldownMs)
        {
            return;
        }
        cooldownActive = false;
    }

    if (retryCount > 0 && (nowMs - lastAttemptMs) < currentRetryInterval)
    {
        return;
    }

    lastAttemptMs = nowMs;
    retryCount++;
    attemptsSinceLastConnection++;
    attemptTracker.recordWiFiAttempt();

    log.print("Wi-Fi connect attempt ");
    log.print(retryCount);
    log.print("/");
    log.print(settings.maxRetries);
    log.print(" (WiFi/all attempts since boot: ");
    log.print(attemptTracker.wifiAttempts());
    log.print("/");
    log.print(attemptTracker.totalAttempts());
    log.println(")");

    WiFi.begin(settings.ssid, settings.password);
    attemptInProgress = true;
}

bool WiFiConnectionHandler::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

uint32_t WiFiConnectionHandler::totalConnectionAttempts() const
{
    return attemptTracker.wifiAttempts();
}
