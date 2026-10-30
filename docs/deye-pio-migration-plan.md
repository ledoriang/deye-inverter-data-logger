# Deye Inverter PlatformIO Migration Plan

**Status:** implementation underway; firmware has not yet been built or validated on the inverter.

## Objective

Replace the ESPHome native-API connection path with a PlatformIO/Arduino firmware for the DFRobot FireBeetle 2 ESP32-C6. The firmware will poll the Deye SUN-12K over Modbus RTU and publish the existing inverter telemetry to Home Assistant through MQTT discovery.

The first release is **read-only** and reproduces the 13 inverter sensors in the current ESPHome configuration. The synthetic `API Connection Test Value` is intentionally excluded. No Modbus write controls are defined by the YAML, so inverter control writes are out of scope.

## Sources and confirmed hardware

- Primary sensor and transport configuration: [`esp_home_config/esp_inverter_v1.yaml`](../esp_home_config/esp_inverter_v1.yaml)
- Historical reference: [`esp_home_config/archive/esp_inverter_v1_ref.yaml`](../esp_home_config/archive/esp_inverter_v1_ref.yaml)
- RS-485 port note: [`README.md`](../README.md)
- PlatformIO project: [`pio/deye_inverter_replica/`](../pio/deye_inverter_replica/)

Confirmed configuration:

| Setting | Value |
|---|---|
| Board / framework | `dfrobot_firebeetle2_esp32c6` / Arduino |
| UART TX / RX | GPIO16 / GPIO17 |
| UART format | 9600 baud, 8 data bits, even parity, 1 stop bit (8E1) |
| RS-485 direction control | GPIO4; confirmed connected to the transceiver control pin with DE and RE bridged |
| Modbus slave address | `0x01` |
| Register type | Holding registers (function 03) |
| Modbus send wait | 100 ms |
| Polling interval | 30 seconds |
| Status LED | GPIO15, non-inverted |
| Inverter port | Meter RS-485 RJ11 port, as noted in the README |

The ESP32's Wi-Fi credentials are for its LAN connection to the MQTT broker and Home Assistant. The inverter link is UART/RS-485; it does not use the Wi-Fi credentials.

## Telemetry to migrate

Preserve the ESPHome register types, units, scale factors, device classes, and state classes. Signed values must be sign-extended as 16-bit integers before publication.

| Home Assistant entity | Register | Decode / scale | Unit | Device class | State class |
|---|---:|---|---|---|---|
| PV1 Power | 672 | Unsigned 16-bit | W | power | measurement |
| PV2 Power | 673 | Unsigned 16-bit | W | power | measurement |
| Inverter Battery SOC | 588 | Unsigned 16-bit | % | battery | measurement |
| Inverter Battery Power | 590 | Signed 16-bit | W | power | measurement |
| Inverter Battery Voltage | 587 | Unsigned 16-bit × 0.1 | V | voltage | measurement |
| Total Load Power | 604 | Signed 16-bit | W | power | measurement |
| Load L1 Power | 601 | Signed 16-bit | W | power | measurement |
| Load L2 Power | 602 | Signed 16-bit | W | power | measurement |
| Load L3 Power | 603 | Signed 16-bit | W | power | measurement |
| Day PV Energy | 676 | Unsigned 16-bit × 0.1 | kWh | energy | total_increasing |
| Day Battery Charge | 592 | Unsigned 16-bit × 0.1 | kWh | energy | total_increasing |
| Day Battery Discharge | 593 | Unsigned 16-bit × 0.1 | kWh | energy | total_increasing |
| Day Load Energy | 607 | Unsigned 16-bit × 0.1 | kWh | energy | total_increasing |

The SOC YAML entry has `skip_updates: 2`. Match its reduced update rate if the Modbus polling design allows it; with a 30-second controller interval this is approximately one update per 90 seconds. The daily energy sensors use `total_increasing` in ESPHome; retain that metadata initially and confirm Home Assistant handles their daily reset behavior as intended.

## Current PlatformIO starting point

- `src/main.cpp` now constructs and wires the services, initializes them, and drives the application loop.
- `ProjectConfig.h` adapts the `Secrets` namespace in `include/secrets.h`; `secrets.example.h` provides the matching template. The local secrets file is git-ignored.
- `ModbusRtuClient` provides holding-register reads with CRC checking and GPIO-controlled half-duplex direction. `DeyeInverterReader` owns the register schedule, signed/unsigned decoding, scaling, and telemetry freshness.
- `WiFiConnectionHandler`, `MqttClientHandler`, and `NetworkAttemptTracker` are reused. `AppServices` coordinates networking, polling, Home Assistant discovery, and the status LED.
- `HaDiscovery` is now inverter-specific in its entity definitions and publishes one retained state topic per sensor. The old incubator entity definitions and missing `config.h` reference have been removed.
- PlatformIO now declares PubSubClient and ArduinoJson. The firmware has not yet been built in the Windows host environment or tested against the inverter/broker.

Reuse the generic networking code after adapting and statically reviewing it in this project. Do not carry forward incubator names, entity IDs, topics, or device metadata. Follow the repository-root [`AGENTS.md`](../AGENTS.md): do not build or flash PlatformIO from this WSL environment; request host-side verification when needed.

## Architecture and module boundaries

Keep `src/main.cpp` as a thin composition root. It should construct long-lived settings, transports, and services; call initialization; and drive the application loop. It should not contain Modbus framing, register decoding, retry policy, Home Assistant discovery JSON, or MQTT topic/payload construction.

Place behavior in focused libraries under `pio/deye_inverter_replica/lib/`:

| Module responsibility | Scope |
|---|---|
| Wi-Fi and MQTT connectivity | Reuse/adapt `WiFiConnectionHandler`, `MqttClientHandler`, and `NetworkAttemptTracker`; preserve their settings-struct configuration and service lifetimes. |
| Modbus RTU transport | Own UART setup, GPIO4 direction control, function-03 transactions, response/CRC handling, and bounded timeouts behind a reusable interface. |
| Deye register model | Define the register catalog, poll groups/schedule, signed and unsigned decoding, scaling, and typed inverter readings. Keep Deye-specific knowledge separate from generic Modbus transport. |
| MQTT state and HA discovery | Keep discovery serialization and MQTT topic publishing in the focused `HaDiscovery` module. Its explicit Deye entity definitions stay out of the transport and `main.cpp`; networking remains reusable through the generic MQTT handler. |
| Application coordination | Coordinate network state, Modbus polling, state publication, and discovery after reconnects; expose a small `begin()`/`loop(nowMs)`-style interface to `main.cpp`. |

`AppServices` is the application coordinator; it owns the Wi-Fi/MQTT service loop, Modbus polling schedule, reconnect discovery, and status indicator. Keep settings and borrowed string/transport lifetimes explicit, following [`docs/NETWORK_MODULES.md`](NETWORK_MODULES.md). Libraries must receive configuration through settings or interfaces, not credentials or hidden project globals. Avoid splitting trivial helpers into needless libraries; split along reusable responsibilities.

## Target firmware design

1. **Configuration and secrets**
   - Define the Wi-Fi settings, MQTT broker endpoint, dedicated MQTT username/password, device identity, and topic prefixes.
   - Create an example configuration for the repository and keep real credentials in a local ignored file or another non-committed mechanism. Update the PlatformIO `.gitignore` before adding local credentials; do not commit broker credentials.
   - Configure persistent objects in an order that satisfies borrowed-string and transport lifetimes in the networking modules.
   - Add explicit, versioned PlatformIO dependencies for MQTT and JSON. Use the `PubSubClient` dependency/version recorded in `docs/NETWORK_MODULES.md` unless compatibility checks require a documented change; the current Modbus transport has no third-party dependency.

2. **Reusable Modbus transport**
   - Use the focused `ModbusRtuClient` interface for ESP32-C6 Arduino UART, 8E1, function-03 holding-register reads, and GPIO4 DE/RE direction control. It is intentionally read-only for this migration.
   - Configure UART pins and serial format from the confirmed settings above. Use the agreed GPIO4 direction pin and allow the configured 100 ms send wait.
   - Encapsulate transaction execution, bounded response timeouts, CRC/error reporting, and diagnostics behind a small transport interface. A failed read must not be reported as a fresh valid value.
   - If host-side build or inverter tests reveal transport limitations, replace the implementation behind this interface with a compatible maintained Modbus library without moving protocol details into `main.cpp`.

3. **Deye register model and polling**
   - Keep the register catalog, poll schedule, decoding, and scaling in a Deye-specific library/module rather than `main.cpp` or generic transport code.
   - Poll the 13 registers on a 30-second cycle using individual reads initially, which isolates failures and avoids assumptions about unlisted registers. Optimize contiguous ranges only after confirming those reads are supported and reliable on the inverter.
   - Return typed readings and read status to the application coordinator; preserve the reduced SOC update rate if practical.

4. **Home Assistant MQTT integration**
   - Use the existing broker and create a separate broker account for this inverter logger. Configure the account to publish the inverter's state and availability topics and Home Assistant MQTT discovery configuration topics, subject to the broker's ACL setup.
   - Use the standard `homeassistant/sensor/<device_id>/<object_id>/config` discovery namespace with stable unique IDs and one device identity for all 13 entities.
   - Publish states at `deye/sun12k/state/<object_id>` and availability at `deye/sun12k/availability`.
   - Publish each sensor value to its own retained state topic and configure discovery metadata to reproduce the table above. Use expiration so a sensor becomes unavailable when its register stops updating. ArduinoJson is used for discovery configuration payloads; size the MQTT packet buffer for the largest discovery message.
   - Configure a retained MQTT Last Will and Testament for offline availability. Publish online availability and (re)publish discovery configuration after every MQTT connection or reconnection.
   - Use MQTT for Home Assistant integration; do not recreate the ESPHome native API or the synthetic random test sensor.

5. **Thin application entry point**
   - Keep `main.cpp` limited to object construction, dependency wiring, setup, and calls into the coordinator's loop.
   - Ensure reusable libraries own their functionality and expose small, reviewable interfaces; keep project-specific register/entity definitions explicit.

6. **Bring-up and cutover**
   - Have the user build/flash from the Windows host as needed; use serial logs to validate the bus, raw register values, decoding, and scaling.
   - Check representative positive and negative signed readings, voltage and energy scaling, SOC cadence, and all 13 entity values against the inverter display or another trusted reading.
   - Verify broker connection, discovery, entity grouping, availability, and recovery after Wi-Fi, broker, and inverter communication interruptions.
   - Only after the PlatformIO device is validated, stop the ESPHome Modbus client before cutover. Do not operate both Modbus masters on the same RS-485 bus concurrently.
   - Consider OTA only after the USB-flashed MQTT version is stable and reachable.

## Implementation sequence

1. **Implemented:** add a non-committed secrets mechanism, project settings, and versioned PubSubClient/ArduinoJson dependencies.
2. **Implemented, pending host verification:** add Modbus RTU transport, Deye decoding/polling, MQTT discovery/state publishing, and application coordination in focused libraries.
3. **Implemented, pending host verification:** reduce `main.cpp` to service construction, dependency wiring, initialization, and loop dispatch.
4. **Pending:** verify the dedicated MQTT broker account and credentials during the host-side connection test; update local `include/secrets.h` if needed.
5. **Pending:** have the user build/flash from the Windows host, validate Modbus readings and Home Assistant discovery, and perform a controlled cutover from ESPHome.

## Completion criteria

- The PlatformIO project builds for the FireBeetle 2 ESP32-C6.
- Modbus communication is stable at 9600 8E1 with slave `0x01` and GPIO4 direction control.
- All 13 YAML inverter sensors appear under one Home Assistant device with correct values, units, and metadata.
- MQTT offline/online availability and reconnect discovery work after network or broker interruption.
- No real Wi-Fi or MQTT credentials are present in tracked files, and no random API test entity is published.
- Firmware remains read-only with respect to inverter registers.
- `main.cpp` contains only application composition and loop orchestration; reusable services and conversion logic live behind focused library interfaces.
