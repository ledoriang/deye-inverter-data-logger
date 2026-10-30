# Project Status

**Last reviewed:** 2026-10-29  
**Migration state:** First implementation slice in place; awaiting host-side build and hardware validation.

## Migration decisions

- Move Home Assistant integration from the ESPHome native API to MQTT discovery using the existing broker.
- Create a dedicated MQTT account for this inverter logger; its credentials must remain out of tracked files.
- Use the confirmed FireBeetle 2 ESP32-C6 wiring: UART TX GPIO16, RX GPIO17, RS-485 direction control GPIO4 (DE and RE bridged), and status LED GPIO15.
- The Wi-Fi credentials are for the ESP32's LAN connection; the inverter is accessed separately over UART/RS-485.
- Communicate with Modbus slave `0x01` at 9600 baud, 8E1, using holding registers and a 30-second polling interval.
- Migrate the 13 inverter sensors described in the active ESPHome YAML. Omit the synthetic `API Connection Test Value`.
- Keep the initial firmware read-only; no inverter write controls are part of the current scope.

## Current implementation state

- `esp_home_config/esp_inverter_v1.yaml` now contains the enabled Modbus and controller configuration, as well as the sensor register map.
- `src/main.cpp` now composes the Wi-Fi, MQTT, Modbus, Deye-reader, and Home Assistant services; behavior is held in focused libraries.
- `ModbusRtuClient` and `DeyeInverterReader` implement Modbus holding-register reads and decoding for the YAML's register map.
- `HaDiscovery` publishes discovery and per-sensor retained state topics; `AppServices` coordinates polling, MQTT availability/reconnect behavior, and the status LED.
- `include/secrets.example.h` documents the `Secrets` namespace. The local `include/secrets.h` now uses that namespace and is git-ignored; it contains the ESP32 LAN Wi-Fi and MQTT broker settings.
- `docs/deye-pio-migration-plan.md` tracks implementation and validation steps. `docs/NETWORK_MODULES.md` describes the reused networking modules and their lifetime/dependency requirements.

## Next steps

1. Confirm the dedicated MQTT broker account accepts the locally configured credentials.
2. Have the user build from the Windows host and resolve any build issues.
3. Validate Modbus readings, MQTT discovery/state/availability, and reconnect behavior on the inverter and in Home Assistant.
4. Perform a controlled cutover from ESPHome after validation.

## Validation status

- Firmware and documents have received static review only.
- Firmware build, hardware communication, broker authentication, and Home Assistant discovery have not yet been validated.
- Follow the PlatformIO build/flash restrictions in the repository-root [`AGENTS.md`](../AGENTS.md) during implementation.
