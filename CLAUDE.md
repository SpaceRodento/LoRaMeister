# LoraMeister - Project Context

## What This Is

General-purpose ESP32 LoRa data exchange platform.
Derived from Zignalmeister 2000, stripped to RYLR890/896 AT-command modules only.
Designed as a reusable base for LoRa sensor projects.

**Current version:** 0.2.0
**Platform:** ESP32 DevKit V1 + RYLR890/RYLR896 (LoRa 868 MHz, AT commands)

## Architecture

```
[Sensor] --> [ESP32 SENDER] --LoRa 868MHz--> [ESP32 RELAY (optional)]
                             --LoRa 868MHz--> [ESP32 RECEIVER] --USB--> [PC/RPi]
```

Three device roles, same firmware, auto-detected via GPIO jumpers at boot:
- **SENDER** — reads sensors, broadcasts data via LoRa
- **RECEIVER** — central hub, logs to USB serial, sends ACKs
- **RELAY** — mesh forwarding node (up to 3 hops)

## Codebase Structure (~7,500 lines)

```
config.h              # ALL feature flags, pins, thresholds
structs.h             # Shared data structures
firmware.ino          # Main program: setup(), loop(), role handlers

# HAL layers
platform_hal.h        # Board detection, pin mapping
debug_hal.h           # Debug output routing (UART)
display_hal.h         # Unified display API (LCD/Serial)
lora_hal.h            # LoRa abstraction (RYLR890 AT commands)

# Communication
lora_handler.h        # RYLR890 AT-command driver
non_blocking_state_v2.h  # Relay state machine (5-state, queue)

# Display
lcd_display.h         # LCD 16x2 I2C (8 layouts)
i2c_manager.h         # I2C bus management

# Sensors
microphone.h          # MAX4466 driver (example sensor)

# System
system_monitoring.h   # Health, RSSI stats, watchdog
power_management.h    # CPU scaling, GPIO power, sleep modes
```

## Coding Conventions

- **Language:** C++ (Arduino framework)
- **Config:** All feature flags in `config.h` — never scatter #defines
- **HAL pattern:** Hardware-specific code goes through HAL layers
- **Memory:** Fixed `char[]` buffers preferred over `String`
- **Blocking:** Never use `delay()` in main loop — use millis()-based patterns
- **Naming:** camelCase functions, UPPER_CASE constants, PascalCase structs

## How to Add Sensors

This is the primary use case. See `microphone.h` as the template:

1. `config.h` — Add `ENABLE_xxx` flag and sensor config
2. Create `xxx_sensor.h` — Init, update, get functions
3. `structs.h` — Add fields to `DeviceState`
4. `firmware.ino` — Add to `buildPayload()` and `parsePayload()`
5. `lcd_display.h` — Add layout (optional)

## Development Workflow

- PlatformIO (VS Code)
- Feature branches with PR-based merges
- Serial Monitor at 115200 baud for debugging
- Test on actual hardware before merging

## What NOT to Do

- Don't add XIAO/SX1262 support — that's Zignalmeister's domain
- Don't over-engineer — this is a learning/prototyping platform
- Keep sensor additions self-contained in their own .h files
