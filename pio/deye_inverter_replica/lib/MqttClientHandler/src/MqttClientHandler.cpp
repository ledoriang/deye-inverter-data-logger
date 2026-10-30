#include "MqttClientHandler.h"

namespace
{
    const char *mqttStateDescription(int state)
    {
        switch (state)
        {
        case MQTT_CONNECTION_TIMEOUT:
            return "MQTT response timeout (CONNACK/PINGRESP)";
        case MQTT_CONNECTION_LOST:
            return "connection lost after connecting";
        case MQTT_CONNECT_FAILED:
            return "TCP connection to broker failed (unreachable, refused, or filtered)";
        case MQTT_DISCONNECTED:
            return "client disconnected";
        case MQTT_CONNECTED:
            return "connected";
        case MQTT_CONNECT_BAD_PROTOCOL:
            return "broker rejected MQTT protocol version";
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return "broker rejected client ID";
        case MQTT_CONNECT_UNAVAILABLE:
            return "broker unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return "broker rejected username or password";
        case MQTT_CONNECT_UNAUTHORIZED:
            return "client is not authorized";
        default:
            return "unknown MQTT error";
        }
    }
}

MqttClientHandler::MqttClientHandler(
    Client &transport,
    const MqttClientSettings &clientSettings,
    NetworkAttemptTracker &connectionAttemptTracker,
    Stream &logOut)
    : settings(clientSettings),
      attemptTracker(connectionAttemptTracker),
      currentRetryInterval(clientSettings.initialRetryIntervalMs),
      log(logOut),
      attemptsSinceLastConnection(0),
      client(transport),
      retryCount(0),
      lastAttemptMs(0),
      cooldownStartedMs(0),
      cooldownActive(false),
      wasConnected(false),
      configurationValid(false)
{
}

bool MqttClientHandler::begin()
{
    retryCount = 0;
    currentRetryInterval = settings.initialRetryIntervalMs;
    cooldownStartedMs = 0;
    cooldownActive = false;
    wasConnected = false;

    if (settings.broker == nullptr || settings.broker[0] == '\0' ||
        settings.port == 0 || settings.clientId == nullptr || settings.clientId[0] == '\0' ||
        settings.maxRetries == 0 ||
        settings.maxRetryIntervalMs < settings.initialRetryIntervalMs ||
        settings.bufferSize == 0 || settings.socketTimeoutSeconds == 0 ||
        settings.willQos > 2 ||
        ((settings.willTopic == nullptr) != (settings.willPayload == nullptr)) ||
        (settings.willTopic != nullptr && settings.willTopic[0] == '\0'))
    {
        log.println("[MQTT] Invalid client settings; MQTT not started");
        configurationValid = false;
        return false;
    }

    client.setServer(settings.broker, settings.port);
    configurationValid = client.setBufferSize(settings.bufferSize);
    client.setSocketTimeout(settings.socketTimeoutSeconds);
    client.setKeepAlive(settings.keepAliveSeconds);

    log.print("[MQTT] Broker endpoint: ");
    log.print(settings.broker);
    log.print(":");
    log.print(settings.port);
    log.print(", client ID: ");
    log.println(settings.clientId);
    log.print("[MQTT] Buffer=");
    log.print(settings.bufferSize);
    log.print(" bytes, socket timeout=");
    log.print(settings.socketTimeoutSeconds);
    log.print(" s, keepalive=");
    log.print(settings.keepAliveSeconds);
    log.println(" s");
    if (!configurationValid)
    {
        log.println("[MQTT] Failed to allocate the configured packet buffer");
    }
    return configurationValid;
}

void MqttClientHandler::loop(uint32_t nowMs, bool networkReady)
{
    if (!configurationValid)
    {
        return;
    }

    if (!networkReady)
    {
        if (client.connected())
        {
            if (settings.willTopic != nullptr && settings.willPayload != nullptr)
            {
                client.publish(
                    settings.willTopic,
                    settings.willPayload,
                    settings.willRetained);
            }
            client.disconnect();
        }

        wasConnected = false;
        return;
    }

    if (client.connected())
    {
        if (!wasConnected)
        {
            logConnectionSuccess();
        }
        wasConnected = true;
        retryCount = 0;
        currentRetryInterval = settings.initialRetryIntervalMs;
        cooldownActive = false;
        if (!client.loop())
        {
            logConnectionLost(client.state());
            wasConnected = false;
        }
        return;
    }

    if (wasConnected)
    {
        logConnectionLost(client.state());
        wasConnected = false;
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
    attemptTracker.recordMqttAttempt();

    log.print("MQTT connect attempt ");
    log.print(retryCount);
    log.print("/");
    log.print(settings.maxRetries);
    log.print(" (MQTT/all attempts since boot: ");
    log.print(attemptTracker.mqttAttempts());
    log.print("/");
    log.print(attemptTracker.totalAttempts());
    log.println(")");

    if (client.connect(
            settings.clientId,
            settings.username,
            settings.password,
            settings.willTopic,
            settings.willQos,
            settings.willRetained,
            settings.willPayload))
    {
        logConnectionSuccess();
        wasConnected = true;
        retryCount = 0;
        currentRetryInterval = settings.initialRetryIntervalMs;
        cooldownActive = false;
        return;
    }

    const int state = client.state();
    log.print("MQTT connect failed, rc=");
    log.print(state);
    log.print(" (");
    log.print(mqttStateDescription(state));
    log.println(")");
    log.print("[MQTT] target=");
    log.print(settings.broker);
    log.print(":");
    log.println(settings.port);
    if (state == MQTT_CONNECT_FAILED)
    {
        log.println("[MQTT] Check that the broker host still has this IP, is online, listens on this port, and allows LAN clients.");
    }
    if (retryCount >= settings.maxRetries)
    {
        cooldownStartedMs = nowMs;
        cooldownActive = settings.cooldownMs > 0;
        retryCount = 0;
        currentRetryInterval = settings.initialRetryIntervalMs;

        log.print("MQTT retries exhausted, backing off for ");
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
}

bool MqttClientHandler::isConnected()
{
    return client.connected();
}

bool MqttClientHandler::publish(const char *topic, const char *payload, size_t payloadLength, bool retained)
{
    if (!configurationValid || !client.connected())
    {
        return false;
    }

    return client.publish(topic, reinterpret_cast<const uint8_t *>(payload), payloadLength, retained);
}

void MqttClientHandler::logConnectionSuccess()
{
    log.println("MQTT connected");
    log.print("[MQTT] Attempts for this connection: ");
    log.print(attemptsSinceLastConnection);
    log.print("; totals since boot (WiFi/MQTT/all): ");
    log.print(attemptTracker.wifiAttempts());
    log.print("/");
    log.print(attemptTracker.mqttAttempts());
    log.print("/");
    log.println(attemptTracker.totalAttempts());
    attemptsSinceLastConnection = 0;
}

void MqttClientHandler::logConnectionLost(int state)
{
    log.print("MQTT connection lost, rc=");
    log.print(state);
    log.print(" (");
    log.print(mqttStateDescription(state));
    log.println(")");
}

uint32_t MqttClientHandler::totalConnectionAttempts() const
{
    return attemptTracker.mqttAttempts();
}
