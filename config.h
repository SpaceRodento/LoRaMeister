/*=====================================================================
  config.h - Master Configuration File

  LoraMeister - ESP32 LoRa General-Purpose Communication System

  PURPOSE:
  Central configuration file containing ALL feature toggles, pin
  definitions, thresholds, and constants.

  HARDWARE:
  ESP32 DevKit V1 + RYLR890/RYLR896 (AT commands, UART)

  Role Detection (hardware jumpers):
  - RECEIVER: GPIO16 connected to GPIO17 (jumper wire)
  - SENDER:   GPIO16 floating (no connection)
  - RELAY:    GPIO19 connected to GPIO20 (jumper wire)
=======================================================================*/

#ifndef CONFIG_H
#define CONFIG_H

//=============================================================================
// QUICK SETTINGS
//=============================================================================
#define QS_LCD_LAYOUT              1       // 1=Minimalist, 2=LoRa Range, 8=Microphone
#define QS_LORA_SEND_INTERVAL_MS   3500    // Send interval (ms)
#define QS_ACK_INTERVAL            30      // ACK every Nth packet
#define QS_DEBUG_MIC               false   // MAX4466 debug to Serial

//=============================================================================
// PROJECT INFO
//=============================================================================
#define PROJECT_NAME "LoraMeister"
#define PROJECT_VERSION "0.1.0"
#define PROJECT_VERSION_MAJOR 0
#define PROJECT_VERSION_MINOR 1
#define PROJECT_VERSION_PATCH 0
#define PROJECT_DATE "2026-03-19"

// Hardware platform (fixed: DevKit + RYLR890)
#define USE_XIAO_SX1262 false

//=============================================================================
// PIN DEFINITIONS - ESP32 DevKit V1 + RYLR890/896
//=============================================================================
//
// GPIO  | Function              | Type        | Notes
// ------|----------------------|-------------|---------------------------
// 2     | LED_PIN              | Output      | Built-in LED
// 16    | MODE_SELECT_PIN      | Input       | Receiver detect
// 17    | MODE_GND_PIN         | Output      | Receiver detect GND
// 19    | RELAY_MODE_PIN_A     | Input       | Relay detect
// 20    | RELAY_MODE_PIN_B     | Output      | Relay detect GND
// 21    | I2C SDA              | I2C         | LCD display
// 22    | I2C SCL              | I2C         | LCD display
// 25    | RXD2 (LoRa RX)      | UART        | RYLR890 TX -> ESP32
// 26    | TXD2 (LoRa TX)      | UART        | RYLR890 RX <- ESP32
// 34    | MIC_PIN              | ADC Input   | MAX4466 analog out
//
// SAFE PINS FOR NEW SENSORS:
// GPIO 4, 5, 12, 13, 14, 15, 18, 23, 27, 32, 33 (output capable)
// GPIO 34, 35, 36, 39 (input-only, good for ADC)

// ── LoRa UART ──
#define RXD2 25                     // RYLR890 TX -> ESP32
#define TXD2 26                     // RYLR890 RX <- ESP32

// ── LED ──
#define LED_PIN 2                   // Built-in LED

// ── Mode Detection ──
#define MODE_SELECT_PIN 16          // RECEIVER detect (INPUT_PULLUP)
#define MODE_GND_PIN 17             // RECEIVER GND reference (OUTPUT LOW)
#define RELAY_MODE_PIN_A 19         // RELAY detect (INPUT_PULLUP)
#define RELAY_MODE_PIN_B 20         // RELAY GND reference (OUTPUT LOW)

// ── LCD Display (I2C 16x2) ──
#define I2C_SDA_PIN 21              // I2C Data
#define I2C_SCL_PIN 22              // I2C Clock

// ── Touch Sensor ──
#define TOUCH_PIN T0                // GPIO 4

// ── MAX4466 Microphone ──
#define MIC_PIN 34                  // ADC1_CH6 (input-only GPIO, perfect for ADC)

// ── LM393 Light Sensor (optional) ──
#define LIGHT_PIN 34                // LM393 DO pin (shares with MIC if not using LM393)
#define LIGHT_ANALOG_PIN 32         // LM393 AO pin - ADC1_CH4
#define LIGHT_VCC_PIN 33            // GPIO power VCC
#define LIGHT_GND_PIN 27            // GPIO power GND

// ── Proximity (not used, placeholder) ──
#define PROXIMITY_PIN 12
#define PROXIMITY_VCC_PIN 13
#define PROXIMITY_GND_PIN 14

// ── Relay Address ──
#define LORA_RELAY_ADDRESS 4

//=============================================================================
// LoRa CONFIGURATION
//=============================================================================
#define LORA_RECEIVER_ADDRESS 1
#define LORA_SENDER_ADDRESS 2
#define LORA_DISPLAY_ADDRESS 3
#define LORA_NETWORK_ID 6         // SAME ON ALL DEVICES!
#define LORA_BAUDRATE 115200
#define LORA_BAND 868             // 868 MHz (Europe)
#define LORA_NETWORK_ID_VALUE 6
#define LORA_BROADCAST_ADDR 0

//=============================================================================
// MESH NETWORK
//=============================================================================
#define ENABLE_MESH_NETWORK true
#define MESH_MAX_HOPS 3
#define RELAY_FORWARD_DELAY_MS 100
#define RELAY_STATS_REPORT_INTERVAL 30000
#define MESH_BROADCAST_ID 0

#define ENABLE_MESH_DEDUPLICATION true
#define DEDUP_BUFFER_SIZE 500
#define DEDUP_TIMEOUT_MS 10000

#define ENABLE_MESH_SOURCE_TRACKING true
#define ENABLE_MESH_RSSI_FILTER true
#define MESH_RELAY_MIN_RSSI -120

#define ENABLE_MESH_ENCRYPTION false
#define MESH_ENCRYPTION_KEY 0xA7B3C9D2

#define MESH_RELAY_BEACON true
#define MESH_BEACON_PRIORITY true
#define ENABLE_DUAL_MODE_RELAY false

//=============================================================================
// BIDIRECTIONAL COMMUNICATION (TDMA)
//=============================================================================
#define ENABLE_BIDIRECTIONAL_TDMA true
#define BEACON_LISTEN_WINDOW_MS 450
#define BEACON_MAX_WAIT_MS 4000
#define BEACON_MESSAGE "BEACON:READY"

#define ENABLE_COMMAND_RESPONSE true
#define COMMAND_TIMEOUT_MS 5000
#define COMMAND_RETRY_COUNT 2
#define COMMAND_LISTEN_INTERVAL_MS 500
#define COMMAND_LISTEN_DURATION_MS 100

#define ENABLE_COMMAND_DEDUPLICATION true
#define COMMAND_DEDUP_TIMEOUT_MS 5000

#define ENABLE_BIDIRECTIONAL true
#define ACK_INTERVAL QS_ACK_INTERVAL
#define LISTEN_TIMEOUT 150

//=============================================================================
// AUTO-ADDRESS & SEND INTERVAL
//=============================================================================
#define ENABLE_AUTO_ADDRESS true
#define AUTO_ADDRESS_MIN 10
#define AUTO_ADDRESS_MAX 250

#define LORA_SEND_INTERVAL_MS QS_LORA_SEND_INTERVAL_MS

// TX retry
#define ENABLE_TX_RETRY true
#define TX_MAX_RETRIES 3
#define TX_RETRY_BACKOFF_MS 100

//=============================================================================
// COMMUNICATION
//=============================================================================
#define SERIAL2_BAUDRATE 115200
#define MAX_RX_BUFFER 256
#define RX_TIMEOUT_WARNING 5000

// CSV/JSON output
#define ENABLE_CSV_OUTPUT true
#define ENABLE_JSON_OUTPUT false
#define DATA_OUTPUT_INTERVAL 2000

//=============================================================================
// DISPLAY
//=============================================================================
// LCD only (no TFT in this project)
#define DISPLAY_MODE 1              // 1 = LCD only
#define FORCE_LCD false

#define ENABLE_LCD              (DISPLAY_MODE == 1 || DISPLAY_MODE == 2)
#define ENABLE_DISPLAY_OUTPUT   false  // No TFT

#define LCD_LAYOUT_NUMBER QS_LCD_LAYOUT
#define LCD_SENDER_ENABLED true

// TFT placeholders (not used, but referenced by display_hal)
#define DISPLAY_TX_PIN 23
#define DISPLAY_RX_PIN 22
#define DISPLAY_UPDATE_INTERVAL 2000
#define DISPLAY_DETECT_TIMEOUT 2000
#define ENABLE_DISPLAY_AUTODETECT false

//=============================================================================
// ESSENTIAL FEATURES
//=============================================================================
#define ENABLE_WATCHDOG true
#define WATCHDOG_TIMEOUT_S 10

#define ENABLE_LED_STATUS true
#define LED_STATUS_BOOT_BLINKS 5
#define LED_STATUS_BOOT_DELAY 100
#define LED_STATUS_ERROR_DELAY 200

#define ENABLE_DEVICE_ID_BLINK true
#define DEVICE_ID_BLINK_DELAY 200
#define DEVICE_ID_DIGIT_PAUSE 500
#define DEVICE_ID_FINAL_PAUSE 1000

#define ENABLE_NVS_STORAGE true
#define NVS_NAMESPACE "lorameister"

// OTA disabled
#define ENABLE_OTA false

#define ENABLE_PERFORMANCE_MONITOR true
#define PERF_REPORT_INTERVAL 60000

//=============================================================================
// SENSORS
//=============================================================================

// ── Touch Sensor ──
#define ENABLE_TOUCH_SENSOR true
#define ENABLE_TOUCH_COMMAND true
#define TOUCH_THRESHOLD 50
#define TOUCH_LOGIC_INVERTED false
#define TOUCH_SENSOR_DEBUG false
#define TOUCH_COMMAND_ACTION "LED_BLINK:3"
#define TOUCH_COMMAND_COOLDOWN_MS 2000

// ── Battery Monitor (disabled) ──
#define ENABLE_BATTERY_MONITOR false
#define BATTERY_PIN 35
#define BATTERY_VOLTAGE_DIVIDER 2.0
#define BATTERY_CHECK_INTERVAL 60000
#define BATTERY_LOW_THRESHOLD 3.3
#define BATTERY_CRITICAL_THRESHOLD 3.0
#define BATTERY_MIN_VOLTAGE 3.0
#define BATTERY_MAX_VOLTAGE 4.2

// ── Current Monitor (disabled) ──
#define ENABLE_CURRENT_MONITOR false
#define CURRENT_MONITOR_I2C_ADDR 0x40
#define CURRENT_CHECK_INTERVAL 10000
#define CURRENT_HIGH_THRESHOLD 200
#define CURRENT_MAX_THRESHOLD 500

// ── Audio Detection (legacy, for fire alarm sound) ──
#define ENABLE_AUDIO_DETECTION false
#define AUDIO_PIN 34
#define AUDIO_SAMPLES 50
#define AUDIO_THRESHOLD 200
#define AUDIO_PEAK_MIN 2
#define AUDIO_PEAK_MAX 6
#define AUDIO_COOLDOWN 5000

// ── Light Detection (LM393, disabled by default for LoraMeister) ──
#define ENABLE_LIGHT_DETECTION false
#define LM393_BRIGHT_THRESHOLD 1000
#define LM393_DARK_THRESHOLD 3000
#define LIGHT_FLASH_MIN_MS 10
#define LIGHT_FLASH_MAX_MS 500
#define LIGHT_FLASH_INTERVAL_MIN 100
#define LIGHT_FLASH_INTERVAL_MAX 5200
#define LIGHT_FLASH_CONFIRM_COUNT 3
#define LM393_DEBUG_FORCE_HIGH_PRIORITY true
#define LM393_ENABLE_SCHEDULED_WINDOW false
#define LM393_SCHEDULE_INTERVAL_HOURS 1
#define LM393_SCHEDULE_DURATION_SEC 120
#define LM393_ENABLE_DO_TRIGGER false
#define LM393_ENABLE_DELTA_DETECTION false
#define LM393_DELTA_THRESHOLD 200
#define LM393_DELTA_WINDOW_MS 100
#define LM393_ENABLE_FLASH_SEQUENCE false
#define LM393_ENABLE_HYBRID_FILTER false
#define LM393_HYBRID_CONFIRM_THRESHOLD 1000
#define LM393_ENABLE_ADAPTIVE_BASELINE true
#define LM393_BASELINE_WINDOW_SEC 10
#define LM393_BASELINE_DEVIATION 500
#define LM393_HIGH_PRIORITY_TIMEOUT 5000
#define LM393_ENABLE_STATUS_CHECK true
#define LM393_ENABLE_ALARM_PATTERN true
#define LM393_STATUS_FLASH_TIMEOUT 120000
#define DEBUG_LIGHT_SENSOR false
#define LM393_AUTO_DETECT_SENSOR true

// ── Proximity (disabled) ──
#define ENABLE_PROXIMITY_DETECTION false
#define PROXIMITY_SENSOR_TYPE 0
#define PROXIMITY_ANALOG_PIN 35
#define PROXIMITY_TRIGGER_MODE 1
#define PROXIMITY_MIN_DURATION_MS 100
#define PROXIMITY_COOLDOWN_MS 1000
#define PROXIMITY_ACTIVE_HIGH true

// ── MAX4466 Microphone ──
// Analog electret microphone module, reads sound level
// Connect: OUT -> GPIO34 (ADC), VCC -> 3.3V, GND -> GND
// Output: VCC/2 DC bias (~1.65V), audio signal superimposed
// Reading: sample peak-to-peak over window, convert to dB
#define ENABLE_MICROPHONE true
#define MIC_SAMPLE_WINDOW_MS 50     // Sample window (ms) for peak-to-peak
#define MIC_SEND_INTERVAL_MS 500    // How often to include mic data in LoRa payload
#define MIC_NOISE_FLOOR 50          // ADC values below this = silence
#define MIC_DB_MIN 30               // Minimum dB to display (ambient)
#define MIC_DB_MAX 100              // Maximum dB to display (loud)
#define DEBUG_MICROPHONE QS_DEBUG_MIC

//=============================================================================
// ADVANCED
//=============================================================================

// Data buffer (disabled)
#define ENABLE_DATA_BUFFER false
#define BUFFER_MAX_ENTRIES 100
#define BUFFER_ENTRY_MAX_LENGTH 200
#define BUFFER_ENABLE_TIMESTAMPS true

// Flash logger (disabled)
#define ENABLE_FLASH_LOGGER false
#define FLASH_MAX_EVENTS 500
#define FLASH_WRITE_LIMIT_PER_HOUR 100
#define FLASH_MIN_LOG_LEVEL 0

// Extended telemetry (disabled)
#define ENABLE_EXTENDED_TELEMETRY false

// Packet statistics (disabled)
#define ENABLE_PACKET_STATS false
#define PACKET_STATS_INTERVAL 30000

// Power management (disabled for wired power)
#define ENABLE_POWER_SAVE false
#define POWER_SLEEP_MODE 0
#define POWER_LIGHT_SLEEP_MS 0
#define POWER_DEEP_SLEEP_S 28800
#define POWER_GRACE_PERIOD_S 300
#define POWER_STRATEGY_SELECT 0
#define POWER_LAB_PROFILER_ENABLE false
#define POWER_LAB_USE_INA219 false
#define POWER_LAB_PHASE_DURATION_MS 10000

// GPIO power management
#define ENABLE_GPIO_POWER true

#define LCD_USE_GPIO_POWER false
#define LCD_VCC_PIN 19
#define LCD_EXPECTED_CURRENT_MA 15

#define LIGHT_USE_GPIO_POWER false
#define LIGHT_EXPECTED_CURRENT_MA 5

#define PROXIMITY_USE_GPIO_POWER false
#define PROXIMITY_EXPECTED_CURRENT_MA 12

#define AUDIO_USE_GPIO_POWER false
#define AUDIO_VCC_PIN 15
#define AUDIO_EXPECTED_CURRENT_MA 10

// Debug
#define DEBUG_LORA_AT false
#define ENABLE_MANUAL_AT_COMMANDS false
#define DEBUG_STATE_MACHINE true

//=============================================================================
// VALIDATION
//=============================================================================

#if AUTO_ADDRESS_MIN <= 4
  #error "AUTO_ADDRESS_MIN must be > 4"
#endif
#if AUTO_ADDRESS_MAX > 250
  #error "AUTO_ADDRESS_MAX must be <= 250"
#endif

#if LORA_SEND_INTERVAL_MS < 3000
  #warning "LORA_SEND_INTERVAL_MS < 3000ms may cause overlap with SF12"
#endif

// Helper macros
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#pragma message "LoraMeister v" PROJECT_VERSION " - ESP32 DevKit + RYLR890"

#endif // CONFIG_H
