# Project Notes for AI Agents

## Do NOT build / flash via PlatformIO

This workspace is accessed from Linux (WSL), but PlatformIO flashing only works from
the Windows host. Do NOT attempt to build or flash the firmware with `pio run`,
`pio upload`, or similar from this environment. It wastes time and does not work.

- Reason: this instance has no working access to the ESP32 over a serial/USB port.
- If a build/compile check or a flash is genuinely required, ask the user to run it
  from Windows (e.g. via PlatformIO IDE / `pio run` on the host), or to build on WSL
  and flash separately.
- Otherwise rely on static review / reasoning rather than compiling.

## Firmware structure

- Keep `pio/deye_inverter_replica/src/main.cpp` as thin as practical: use it to
  construct and connect services, initialize them, and drive the main loop.
- Put reusable behavior in focused PlatformIO libraries under
  `pio/deye_inverter_replica/lib/`, with clear headers and interfaces. Keep
  reusable networking, Modbus transport, Home Assistant/MQTT publishing, and
  inverter-specific register decoding out of `main.cpp`.
- Keep generic infrastructure separate from Deye-specific register maps and
  scaling. Pass configuration and dependencies through settings/interfaces;
  libraries should not depend on hidden project globals or credentials.
- Avoid unnecessary abstraction: each library should own a cohesive
  responsibility and have an interface that can be reused or independently
  reviewed.
