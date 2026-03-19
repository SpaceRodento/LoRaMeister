/*=====================================================================
  display_hal.h - Display Hardware Abstraction Layer

  Unified display abstraction for Zignalmeister 2000.
  Provides a single API that works across different display types
  with automatic fallback.

  SUPPORTED DISPLAYS:
  1. LCD 16x2 I2C (primary, address 0x27)
  2. TFT UART (secondary, uses DisplayClient protocol)
  3. Serial Console (fallback when no display connected)

  FEATURES:
  - Automatic hardware detection at runtime
  - Unified API for all display types
  - Priority: LCD > TFT > Serial
  - Smart fallback when primary display unavailable
  - Platform-aware (handles XIAO pin limitations)

  USAGE:
    #include "display_hal.h"

    void setup() {
      platform_init();           // Initialize platform first
      display_init();            // Auto-detect and init display

      display_clear();
      display_print(0, "Hello World");
      display_print(1, "Line 2");

      // Or use formatted output
      display_printf(0, "RSSI: %d dBm", rssi);
    }

  API REFERENCE:
    display_init()              - Auto-detect and initialize display
    display_clear()             - Clear all content
    display_print(line, text)   - Print text to line (0 or 1)
    display_printf(line, fmt, ...)  - Printf-style output
    display_update()            - Force update (for TFT buffering)
    display_setBacklight(on)    - Control backlight (LCD only)
    display_getType()           - Get active display type

=======================================================================*/

#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include "platform_hal.h"

// ═══════════════════════════════════════════════════════════════════
// DISPLAY TYPES
// ═══════════════════════════════════════════════════════════════════

typedef enum {
  DISPLAY_NONE = 0,
  DISPLAY_LCD_I2C,       // LCD 16x2 I2C
  DISPLAY_TFT_UART,      // TFT UART (DisplayClient)
  DISPLAY_SERIAL         // Serial console fallback
} DisplayType;

// ═══════════════════════════════════════════════════════════════════
// DISPLAY CONFIGURATION
// ═══════════════════════════════════════════════════════════════════

#define DISPLAY_LCD_ADDR       0x27   // Common I2C address for LCD
#define DISPLAY_LCD_COLS       16     // LCD columns
#define DISPLAY_LCD_ROWS       2      // LCD rows

#define DISPLAY_TFT_BAUDRATE   9600   // TFT UART baudrate

#define DISPLAY_LINE_MAX_LEN   20     // Max characters per line

// ═══════════════════════════════════════════════════════════════════
// DISPLAY STATE
// ═══════════════════════════════════════════════════════════════════

static DisplayType g_displayType = DISPLAY_NONE;
static bool g_displayInitialized = false;

// Line buffers for comparison (avoid redundant updates)
static char g_displayLine0[DISPLAY_LINE_MAX_LEN + 1] = "";
static char g_displayLine1[DISPLAY_LINE_MAX_LEN + 1] = "";

// TFT serial port (platform-dependent)
static HardwareSerial* g_tftSerial = nullptr;

// ═══════════════════════════════════════════════════════════════════
// LCD DRIVER (Minimal I2C implementation)
// ═══════════════════════════════════════════════════════════════════
// Lightweight LCD driver to avoid LiquidCrystal_I2C dependency issues

#define LCD_BACKLIGHT   0x08
#define LCD_ENABLE      0x04
#define LCD_RS          0x01

// LCD command set
#define LCD_CLEAR       0x01
#define LCD_HOME        0x02
#define LCD_ENTRY_MODE  0x06
#define LCD_DISPLAY_ON  0x0C
#define LCD_FUNCTION    0x28  // 4-bit, 2 lines, 5x8

static uint8_t g_lcdBacklight = LCD_BACKLIGHT;

// Send 4 bits to LCD
static void lcd_write4bits(uint8_t data) {
  Wire.beginTransmission(DISPLAY_LCD_ADDR);
  Wire.write(data | g_lcdBacklight);
  Wire.endTransmission();
  delayMicroseconds(1);

  // Pulse enable
  Wire.beginTransmission(DISPLAY_LCD_ADDR);
  Wire.write(data | g_lcdBacklight | LCD_ENABLE);
  Wire.endTransmission();
  delayMicroseconds(1);

  Wire.beginTransmission(DISPLAY_LCD_ADDR);
  Wire.write((data | g_lcdBacklight) & ~LCD_ENABLE);
  Wire.endTransmission();
  delayMicroseconds(50);
}

// Send command to LCD
static void lcd_command(uint8_t cmd) {
  lcd_write4bits(cmd & 0xF0);
  lcd_write4bits((cmd << 4) & 0xF0);
  if (cmd <= 3) delay(2);  // Clear and home need more time
}

// Send data (character) to LCD
static void lcd_data(uint8_t data) {
  lcd_write4bits((data & 0xF0) | LCD_RS);
  lcd_write4bits(((data << 4) & 0xF0) | LCD_RS);
}

// Initialize LCD
static bool lcd_init() {
  const PlatformPins* pins = platform_getPins();

  // Initialize I2C
  Wire.begin(pins->i2cSda, pins->i2cScl);
  delay(50);

  // Check if LCD is present
  Wire.beginTransmission(DISPLAY_LCD_ADDR);
  if (Wire.endTransmission() != 0) {
    return false;  // LCD not found
  }

  // 4-bit initialization sequence
  delay(15);
  lcd_write4bits(0x30);
  delay(5);
  lcd_write4bits(0x30);
  delayMicroseconds(150);
  lcd_write4bits(0x30);
  lcd_write4bits(0x20);  // Set 4-bit mode

  // Configure LCD
  lcd_command(LCD_FUNCTION);    // 4-bit, 2 lines
  lcd_command(LCD_DISPLAY_ON);  // Display on, cursor off
  lcd_command(LCD_CLEAR);       // Clear display
  lcd_command(LCD_ENTRY_MODE);  // Increment, no shift

  return true;
}

// Clear LCD
static void lcd_clear() {
  lcd_command(LCD_CLEAR);
}

// Set cursor position
static void lcd_setCursor(uint8_t col, uint8_t row) {
  uint8_t offset = row == 0 ? 0x00 : 0x40;
  lcd_command(0x80 | (col + offset));
}

// Print string to LCD
static void lcd_print(const char* str) {
  while (*str) {
    lcd_data(*str++);
  }
}

// Set backlight
static void lcd_setBacklight(bool on) {
  g_lcdBacklight = on ? LCD_BACKLIGHT : 0;
  Wire.beginTransmission(DISPLAY_LCD_ADDR);
  Wire.write(g_lcdBacklight);
  Wire.endTransmission();
}

// ═══════════════════════════════════════════════════════════════════
// TFT UART DRIVER (DisplayClient protocol compatible)
// ═══════════════════════════════════════════════════════════════════

static bool tft_init() {
  const PlatformPins* pins = platform_getPins();

  if (pins->displayTx < 0) {
    return false;
  }

  // On ESP32, use Serial2. On XIAO, Serial1 or fallback
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    // XIAO ESP32S3: TFT uses D4/D5 (GPIO 5/6) via Serial1
    // These pins are shared with I2C (LCD) - mutually exclusive via DISPLAY_MODE
    Serial1.begin(DISPLAY_TFT_BAUDRATE, SERIAL_8N1, pins->displayRx, pins->displayTx);
    g_tftSerial = &Serial1;
  #else
    // ESP32 DevKit: Use Serial2
    Serial2.begin(DISPLAY_TFT_BAUDRATE, SERIAL_8N1, pins->displayRx, pins->displayTx);
    g_tftSerial = &Serial2;
  #endif

  delay(100);

  // Send init ping
  g_tftSerial->println("INIT:1");

  return true;
}

static void tft_clear() {
  if (g_tftSerial) {
    g_tftSerial->println("CLEAR:1");
  }
}

static void tft_print(int line, const char* text) {
  if (g_tftSerial) {
    g_tftSerial->print("L");
    g_tftSerial->print(line);
    g_tftSerial->print(":");
    g_tftSerial->println(text);
  }
}

static void tft_update() {
  if (g_tftSerial) {
    g_tftSerial->println("UPDATE:1");
  }
}

// ═══════════════════════════════════════════════════════════════════
// SERIAL FALLBACK DRIVER
// ═══════════════════════════════════════════════════════════════════

static void serial_print(int line, const char* text) {
  Serial.print(F("[DISPLAY L"));
  Serial.print(line);
  Serial.print(F("] "));
  Serial.println(text);
}

// ═══════════════════════════════════════════════════════════════════
// DISPLAY HAL PUBLIC API
// ═══════════════════════════════════════════════════════════════════

/**
 * Initialize display system
 *
 * Behavior depends on DISPLAY_MODE:
 * - Mode 0: No display
 * - Mode 1: Auto-detect LCD, fallback to Serial
 * - Mode 2 (auto): Serial only at this stage; call display_initForRole()
 *   after role detection to initialize the correct display hardware
 *
 * @return true if any display initialized
 */
bool display_init() {
  if (g_displayInitialized) {
    return true;
  }

  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("         DISPLAY HAL - Auto Detection                 "));
  Serial.println(F("======================================================="));

#if DISPLAY_MODE == 2
  // Auto mode: defer hardware init until role is known (display_initForRole)
  Serial.println(F("DISPLAY_MODE=2 (Auto): waiting for role detection..."));
  Serial.println(F("Display hardware will be initialized after role detect"));
  g_displayType = DISPLAY_SERIAL;
  g_displayInitialized = true;
  Serial.println(F("=======================================================\n"));
  return true;

#else
  // Legacy modes (0 and 1): immediate detection

  // Try LCD first (highest priority)
  if (platform_hasCapability(CAP_LCD_I2C)) {
    Serial.print(F("Scanning for LCD at 0x"));
    Serial.print(DISPLAY_LCD_ADDR, HEX);
    Serial.print(F("... "));

    if (lcd_init()) {
      Serial.println(F("FOUND"));
      g_displayType = DISPLAY_LCD_I2C;
      g_displayInitialized = true;

      // Show init message
      lcd_clear();
      lcd_setCursor(0, 0);
      lcd_print("Zignalmeister");
      lcd_setCursor(0, 1);
      lcd_print("2000 v2.7");

      Serial.println(F("LCD I2C 16x2 initialized"));
      Serial.println(F("=======================================================\n"));
      return true;
    }
    Serial.println(F("not found"));
  }

  // Try TFT (second priority)
  if (platform_hasCapability(CAP_TFT_UART)) {
    Serial.print(F("Initializing TFT UART... "));

    if (tft_init()) {
      Serial.println(F("OK"));
      g_displayType = DISPLAY_TFT_UART;
      g_displayInitialized = true;

      tft_clear();
      tft_print(0, "Zignalmeister 2000");
      tft_print(1, "TFT Display Ready");
      tft_update();

      Serial.println(F("TFT UART display initialized"));
      Serial.println(F("=======================================================\n"));
      return true;
    }
    Serial.println(F("failed"));
  }

  // Fallback to Serial console
  Serial.println(F("No hardware display found"));
  Serial.println(F("Using Serial console as display fallback"));
  g_displayType = DISPLAY_SERIAL;
  g_displayInitialized = true;

  Serial.println(F("=======================================================\n"));
  return true;
#endif // DISPLAY_MODE
}

// Forward declaration (defined below, needed by display_initForRole)
const char* display_getTypeName();

// ═══════════════════════════════════════════════════════════════════
// ROLE-BASED DISPLAY INITIALIZATION (DISPLAY_MODE=2 auto)
// ═══════════════════════════════════════════════════════════════════

/**
 * Initialize display based on device role (Option A)
 *
 * Called after role detection (bRECEIVER/bRELAY known).
 * Sets g_displayType which controls all subsequent display operations.
 *
 * Logic:
 *   FORCE_LCD=true → I2C probe for LCD (any role)
 *   RECEIVER       → TFT (UART), no I2C probe
 *   SENDER/RELAY   → I2C probe at 0x27: LCD if found, NONE if not
 *
 * @param isReceiver true if device is RECEIVER
 * @param isRelay true if device is RELAY
 * @return true if display type was determined
 */
bool display_initForRole(bool isReceiver, bool isRelay) {
#if DISPLAY_MODE != 2
  return false;  // Only for auto mode
#else

  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("   DISPLAY AUTO-DETECT (Role-Based, Option A)         "));
  Serial.println(F("======================================================="));

  const char* roleName = isReceiver ? "RECEIVER" : (isRelay ? "RELAY" : "SENDER");
  Serial.print(F("Device role: "));
  Serial.println(roleName);

#if FORCE_LCD
  // FORCE_LCD override: always probe for LCD regardless of role
  Serial.println(F("FORCE_LCD=true: probing I2C for LCD..."));

  if (lcd_init()) {
    g_displayType = DISPLAY_LCD_I2C;
    Serial.println(F("LCD found at 0x27 - LCD forced ON"));

    // Show splash on HAL LCD
    lcd_clear();
    lcd_setCursor(0, 0);
    lcd_print("Zignalmeister");
    lcd_setCursor(0, 1);
    lcd_print("LCD (forced)");
  } else {
    g_displayType = DISPLAY_SERIAL;
    Serial.println(F("LCD not found at 0x27 - Serial fallback"));
  }

#else
  if (isReceiver) {
    // ── RECEIVER: TFT display (power available, dedicated display) ──
    Serial.println(F("RECEIVER -> TFT display (UART)"));
    Serial.println(F("TFT will be initialized by initDisplaySender()"));
    g_displayType = DISPLAY_TFT_UART;

    // Note: actual TFT UART init happens in initDisplaySender()
    // We just set the type here so firmware.ino knows to call it

  } else {
    // ── SENDER/RELAY: probe I2C for LCD at 0x27 ──
    Serial.print(roleName);
    Serial.println(F(" -> probing I2C for LCD at 0x27..."));

    if (lcd_init()) {
      g_displayType = DISPLAY_LCD_I2C;
      Serial.println(F("LCD FOUND - LCD display enabled"));

      // Show splash on HAL LCD
      lcd_clear();
      lcd_setCursor(0, 0);
      lcd_print("Zignalmeister");
      lcd_setCursor(0, 1);
      lcd_print(isRelay ? "RELAY mode" : "SENDER mode");
    } else {
      g_displayType = DISPLAY_SERIAL;
      Serial.println(F("LCD NOT FOUND - display disabled (Serial only)"));
      Serial.println(F("Connect LCD to I2C (0x27) and restart to enable"));
    }
  }
#endif // FORCE_LCD

  Serial.print(F("Active display: "));
  Serial.println(display_getTypeName());
  Serial.println(F("=======================================================\n"));

  return true;
#endif // DISPLAY_MODE
}

/**
 * Get active display type
 */
DisplayType display_getType() {
  return g_displayType;
}

/**
 * Get display type as string
 */
const char* display_getTypeName() {
  switch (g_displayType) {
    case DISPLAY_LCD_I2C:  return "LCD I2C";
    case DISPLAY_TFT_UART: return "TFT UART";
    case DISPLAY_SERIAL:   return "Serial Console";
    default:               return "None";
  }
}

/**
 * Clear display
 */
void display_clear() {
  switch (g_displayType) {
    case DISPLAY_LCD_I2C:
      lcd_clear();
      break;
    case DISPLAY_TFT_UART:
      tft_clear();
      break;
    case DISPLAY_SERIAL:
      Serial.println(F("[DISPLAY] --- CLEAR ---"));
      break;
    default:
      break;
  }

  // Clear line buffers
  g_displayLine0[0] = '\0';
  g_displayLine1[0] = '\0';
}

/**
 * Print text to a specific line
 *
 * @param line Line number (0 or 1)
 * @param text Text to display (max 16/20 chars depending on display)
 */
void display_print(int line, const char* text) {
  if (!g_displayInitialized) {
    display_init();
  }

  // Clamp line number
  line = (line < 0) ? 0 : (line > 1 ? 1 : line);

  // Check if text changed (avoid redundant updates)
  char* lineBuffer = (line == 0) ? g_displayLine0 : g_displayLine1;
  if (strcmp(lineBuffer, text) == 0) {
    return;  // No change
  }

  // Update buffer
  strncpy(lineBuffer, text, DISPLAY_LINE_MAX_LEN);
  lineBuffer[DISPLAY_LINE_MAX_LEN] = '\0';

  // Send to display
  switch (g_displayType) {
    case DISPLAY_LCD_I2C:
      lcd_setCursor(0, line);
      // Pad with spaces to clear old content
      char padded[DISPLAY_LCD_COLS + 1];
      snprintf(padded, sizeof(padded), "%-16s", text);
      lcd_print(padded);
      break;

    case DISPLAY_TFT_UART:
      tft_print(line, text);
      break;

    case DISPLAY_SERIAL:
      serial_print(line, text);
      break;

    default:
      break;
  }
}

/**
 * Printf-style formatted output to display line
 *
 * @param line Line number (0 or 1)
 * @param format Printf format string
 * @param ... Format arguments
 */
void display_printf(int line, const char* format, ...) {
  char buffer[DISPLAY_LINE_MAX_LEN + 1];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  display_print(line, buffer);
}

/**
 * Force display update (mainly for TFT buffered output)
 */
void display_update() {
  if (g_displayType == DISPLAY_TFT_UART) {
    tft_update();
  }
}

/**
 * Control backlight (LCD only)
 *
 * @param on true = backlight on, false = off
 */
void display_setBacklight(bool on) {
  if (g_displayType == DISPLAY_LCD_I2C) {
    lcd_setBacklight(on);
  }
}

/**
 * Check if display is available
 */
bool display_isAvailable() {
  return g_displayInitialized && (g_displayType != DISPLAY_NONE);
}

/**
 * Check if hardware display (not Serial fallback)
 */
bool display_isHardware() {
  return g_displayType == DISPLAY_LCD_I2C || g_displayType == DISPLAY_TFT_UART;
}

// ═══════════════════════════════════════════════════════════════════
// DISPLAY LAYOUTS (Common patterns)
// ═══════════════════════════════════════════════════════════════════

/**
 * Display LoRa signal status
 * Line 0: "RSSI:-67 SNR:10"
 * Line 1: "Pkts:123 Loss:2%"
 */
void display_loraStatus(int rssi, int snr, int packets, int lossPercent) {
  display_printf(0, "RSSI:%d SNR:%d", rssi, snr);
  display_printf(1, "Pkts:%d Loss:%d%%", packets, lossPercent);
}

/**
 * Display device info
 * Line 0: "Addr:184 SENDER"
 * Line 1: "TX:45 RX:123"
 */
void display_deviceInfo(uint8_t addr, const char* role, unsigned long tx, unsigned long rx) {
  display_printf(0, "Addr:%d %s", addr, role);
  display_printf(1, "TX:%lu RX:%lu", tx, rx);
}

/**
 * Display startup message
 */
void display_startup(const char* version) {
  display_clear();
  display_print(0, "Zignalmeister");
  display_printf(1, "v%s", version);
}

/**
 * Display error message
 */
void display_error(const char* msg) {
  display_clear();
  display_print(0, "ERROR:");
  display_print(1, msg);
}

// ═══════════════════════════════════════════════════════════════════
// DEBUG INFO
// ═══════════════════════════════════════════════════════════════════

/**
 * Print display HAL information
 */
void display_printInfo() {
  Serial.println();
  Serial.println(F("======================================================="));
  Serial.println(F("         DISPLAY HAL INFORMATION                      "));
  Serial.println(F("======================================================="));
  Serial.print(F("Display Type:   ")); Serial.println(display_getTypeName());
  Serial.print(F("Initialized:    ")); Serial.println(g_displayInitialized ? "Yes" : "No");
  Serial.print(F("Is Hardware:    ")); Serial.println(display_isHardware() ? "Yes" : "No");

  if (g_displayType == DISPLAY_LCD_I2C) {
    Serial.print(F("I2C Address:    0x")); Serial.println(DISPLAY_LCD_ADDR, HEX);
    Serial.print(F("Size:           ")); Serial.print(DISPLAY_LCD_COLS);
    Serial.print("x"); Serial.println(DISPLAY_LCD_ROWS);
  } else if (g_displayType == DISPLAY_TFT_UART) {
    Serial.print(F("UART Baudrate:  ")); Serial.println(DISPLAY_TFT_BAUDRATE);
  }

  Serial.println(F("=======================================================\n"));
}

#endif // DISPLAY_HAL_H
