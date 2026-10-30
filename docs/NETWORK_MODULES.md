# Reusing the Wi-Fi and MQTT modules

`WiFiConnectionHandler` and `MqttClientHandler` are application-configured
helpers for Arduino projects. Neither implementation includes this project's
`config.h`; all project-specific options are supplied through settings structs.
For reuse, copy the `WiFiConnectionHandler`, `MqttClientHandler`, and
`NetworkAttemptTracker` library folders.

## Dependencies

- `WiFiConnectionHandler` uses the platform's `WiFi.h` API (currently Arduino-ESP32).
- `MqttClientHandler` uses `PubSubClient` and accepts any Arduino `Client`
  transport, such as `WiFiClient`, `WiFiClientSecure`, or an Ethernet client.
- Both handlers use the small `NetworkAttemptTracker` library to share counters.
- For PlatformIO, add `knolleary/PubSubClient @ ^2.8` to `lib_deps`.

## Example

```cpp
#include <WiFi.h>
#include <NetworkAttemptTracker.h>
#include <WiFiConnectionHandler.h>
#include <MqttClientHandler.h>

WiFiConnectionSettings wifiSettings = [] {
    WiFiConnectionSettings settings;
    settings.ssid = "your-ssid";
    settings.password = "your-password";
    settings.hostname = "sensor-1";
    return settings;
}();

MqttClientSettings mqttSettings = [] {
    MqttClientSettings settings;
    settings.broker = "192.168.1.10";
    settings.port = 1883;
    settings.clientId = "sensor-1";
    settings.username = "mqtt-user";
    settings.password = "mqtt-password";
    settings.willTopic = "sensors/sensor-1/availability";
    settings.willPayload = "offline";
    settings.willRetained = true;
    settings.bufferSize = 512;
    return settings;
}();

NetworkAttemptTracker attempts;
WiFiClient mqttTransport;
WiFiConnectionHandler wifi(wifiSettings, attempts, Serial);
MqttClientHandler mqtt(mqttTransport, mqttSettings, attempts, Serial);

void setup() {
    wifi.begin();
    mqtt.begin();
}

void loop() {
    const uint32_t now = millis();
    wifi.loop(now);
    mqtt.loop(now, wifi.isConnected());

    // mqtt.publish(topic, payload, payloadLength, retained);
}
```

Handlers copy their settings at construction, so configure the settings first.
String fields are borrowed rather than copied, so their storage and the
transport object must outlive the handlers. The MQTT buffer must be large
enough for the complete MQTT packet being published.

The Wi-Fi handler scans asynchronously at startup by default. It logs each
visible SSID, RSSI, channel, auth-mode code, and access-point BSSID, marks the
configured SSID, and logs the BSSID it actually associates with. The handler
leaves AP selection to the ESP32/mesh network, configured by default for
all-channel scanning and RSSI sorting so it can choose the strongest matching
AP. `scanMethod` and `sortMethod` are configurable in
`WiFiConnectionSettings`. Set
`wifiSettings.scanForNetworksOnStart = false` to disable the diagnostic startup
scan.

Pass one shared `NetworkAttemptTracker` to both handlers to get Wi-Fi, MQTT,
and aggregate attempt totals. Counts are cumulative for the current boot and
are reported on each attempt, on successful connection, and in `[NET]` status
lines.
