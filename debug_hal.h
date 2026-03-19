/*=====================================================================
  debug_hal.h - Debug Output Hardware Abstraction Layer

  Intelligent debug output routing for Zignalmeister 2000.
  Automatically selects the best available debug interface based on
  platform and peripheral usage.

  KEY FEATURE: USB CDC on ESP32S3
  On XIAO ESP32S3, the native USB port provides CDC serial that is
  SEPARATE from hardware UART. This means:
  - TFT can use D0/D2 (hardware UART TX/RX)
  - Debug output goes to USB CDC (native USB port)
  - Both work simultaneously!

  INTERFACE SELECTION:
  ┌─────────────────────────────────────────────────────────┐
  │ Platform      │ TFT Active │ Debug Interface           │
  ├───────────────┼────────────┼───────────────────────────┤
  │ ESP32 DevKit  │ No         │ Serial (UART0)            │
  │ ESP32 DevKit  │ Yes        │ Serial (UART0)            │
  │ XIAO ESP32S3  │ No         │ Serial (USB CDC)          │
  │ XIAO ESP32S3  │ Yes        │ Serial (USB CDC) - OK!    │
  └─────────────────────────────────────────────────────────┘

  USAGE:
    #include "debug_hal.h"

    void setup() {
      debug_init();

      DEBUG_PRINT("Hello debug world");
      DEBUG_PRINTF("Value: %d", 42);
      DEBUG_PRINTLN("With newline");
    }

  MACROS:
    DEBUG_PRINT(msg)           - Print without newline
    DEBUG_PRINTLN(msg)         - Print with newline
    DEBUG_PRINTF(fmt, ...)     - Printf-style output
    DEBUG_HEX(val)             - Print hex value
    DEBUG_BIN(val)             - Print binary value

  API:
    debug_init()               - Initialize debug system
    debug_println(msg)         - Print with newline
    debug_printf(fmt, ...)     - Printf-style output
    debug_isAvailable()        - Check if debug works
    debug_getStream()          - Get underlying Stream*

=======================================================================*/

#ifndef DEBUG_HAL_H
#define DEBUG_HAL_H

#include <Arduino.h>
#include "platform_hal.h"

// ═══════════════════════════════════════════════════════════════════
// DEBUG CONFIGURATION
// ═══════════════════════════════════════════════════════════════════

// Default baudrate for debug output
#ifndef DEBUG_BAUDRATE
  #define DEBUG_BAUDRATE 115200
#endif

// Enable/disable debug at compile time (set to 0 for production)
#ifndef DEBUG_ENABLED
  #define DEBUG_ENABLED 1
#endif

// Debug verbosity levels
#define DEBUG_LEVEL_NONE    0   // No debug output
#define DEBUG_LEVEL_ERROR   1   // Errors only
#define DEBUG_LEVEL_WARN    2   // Errors + warnings
#define DEBUG_LEVEL_INFO    3   // Errors + warnings + info
#define DEBUG_LEVEL_VERBOSE 4   // Everything

#ifndef DEBUG_LEVEL
  #define DEBUG_LEVEL DEBUG_LEVEL_INFO
#endif

// ═══════════════════════════════════════════════════════════════════
// DEBUG INTERFACE TYPES
// ═══════════════════════════════════════════════════════════════════

typedef enum {
  DEBUG_INTERFACE_NONE = 0,
  DEBUG_INTERFACE_UART,        // Hardware UART (Serial)
  DEBUG_INTERFACE_USB_CDC      // USB CDC (native USB on ESP32S3)
} DebugInterfaceType;

// ═══════════════════════════════════════════════════════════════════
// DEBUG STATE
// ═══════════════════════════════════════════════════════════════════

static DebugInterfaceType g_debugInterface = DEBUG_INTERFACE_NONE;
static Stream* g_debugStream = nullptr;
static bool g_debugInitialized = false;
static bool g_tftUsingUart = false;

// ═══════════════════════════════════════════════════════════════════
// INTERNAL FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

/**
 * Detect best debug interface for current platform
 */
static DebugInterfaceType debug_detectInterface() {
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3 (XIAO): Always use USB CDC
    // USB CDC is separate from UART, so it works even if TFT uses UART
    return DEBUG_INTERFACE_USB_CDC;
  #else
    // Standard ESP32: Use hardware UART
    return DEBUG_INTERFACE_UART;
  #endif
}

// ═══════════════════════════════════════════════════════════════════
// DEBUG HAL PUBLIC API
// ═══════════════════════════════════════════════════════════════════

/**
 * Initialize debug system
 *
 * Automatically selects the best debug interface:
 * - ESP32S3 (XIAO): USB CDC (works alongside TFT on UART)
 * - ESP32: Hardware UART (Serial)
 *
 * @return true if debug initialized successfully
 */
bool debug_init() {
  if (g_debugInitialized) {
    return true;
  }

  // Detect appropriate interface
  g_debugInterface = debug_detectInterface();

  switch (g_debugInterface) {
    case DEBUG_INTERFACE_USB_CDC:
      // ESP32S3 USB CDC - already initialized as "Serial"
      // On ESP32S3, Serial is USB CDC by default
      Serial.begin(DEBUG_BAUDRATE);

      // Wait for USB CDC to be ready (with timeout)
      {
        unsigned long start = millis();
        while (!Serial && (millis() - start < 3000)) {
          delay(10);
        }
      }

      g_debugStream = &Serial;
      break;

    case DEBUG_INTERFACE_UART:
    default:
      // Standard UART
      Serial.begin(DEBUG_BAUDRATE);
      g_debugStream = &Serial;
      break;
  }

  g_debugInitialized = true;

  // Print debug info
  if (g_debugStream) {
    g_debugStream->println();
    g_debugStream->println(F("======================================================="));
    g_debugStream->println(F("         DEBUG HAL - Output Routing                   "));
    g_debugStream->println(F("======================================================="));
    g_debugStream->print(F("Interface:  "));
    switch (g_debugInterface) {
      case DEBUG_INTERFACE_USB_CDC:
        g_debugStream->println(F("USB CDC (native USB)"));
        g_debugStream->println(F("Note: Debug works even with TFT on UART!"));
        break;
      case DEBUG_INTERFACE_UART:
        g_debugStream->println(F("Hardware UART (Serial)"));
        break;
      default:
        g_debugStream->println(F("None"));
    }
    g_debugStream->print(F("Baudrate:   ")); g_debugStream->println(DEBUG_BAUDRATE);
    g_debugStream->println(F("=======================================================\n"));
  }

  return true;
}

/**
 * Notify debug system that TFT is using UART
 * (For informational purposes on ESP32S3)
 */
void debug_notifyTftActive() {
  g_tftUsingUart = true;

  if (g_debugInterface == DEBUG_INTERFACE_USB_CDC && g_debugStream) {
    g_debugStream->println(F("[DEBUG] TFT active on UART - debug continues on USB CDC"));
  }
}

/**
 * Get underlying stream for advanced usage
 */
Stream* debug_getStream() {
  return g_debugStream;
}

/**
 * Check if debug is available
 */
bool debug_isAvailable() {
  return g_debugInitialized && g_debugStream != nullptr;
}

/**
 * Get debug interface type
 */
DebugInterfaceType debug_getInterfaceType() {
  return g_debugInterface;
}

/**
 * Get interface name as string
 */
const char* debug_getInterfaceName() {
  switch (g_debugInterface) {
    case DEBUG_INTERFACE_USB_CDC: return "USB CDC";
    case DEBUG_INTERFACE_UART:    return "UART";
    default:                      return "None";
  }
}

// ═══════════════════════════════════════════════════════════════════
// DEBUG OUTPUT FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

/**
 * Print string (no newline)
 */
void debug_print(const char* msg) {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->print(msg);
    }
  #endif
}

/**
 * Print string with newline
 */
void debug_println(const char* msg) {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->println(msg);
    }
  #endif
}

/**
 * Print empty newline
 */
void debug_newline() {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->println();
    }
  #endif
}

/**
 * Printf-style formatted output
 */
void debug_printf(const char* format, ...) {
  #if DEBUG_ENABLED
    if (!g_debugStream) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    g_debugStream->print(buffer);
  #endif
}

/**
 * Printf-style with newline
 */
void debug_printfln(const char* format, ...) {
  #if DEBUG_ENABLED
    if (!g_debugStream) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    g_debugStream->println(buffer);
  #endif
}

/**
 * Print integer value
 */
void debug_printInt(int value) {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->print(value);
    }
  #endif
}

/**
 * Print hex value
 */
void debug_printHex(uint32_t value) {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->print(F("0x"));
      g_debugStream->print(value, HEX);
    }
  #endif
}

/**
 * Print binary value
 */
void debug_printBin(uint32_t value) {
  #if DEBUG_ENABLED
    if (g_debugStream) {
      g_debugStream->print(F("0b"));
      g_debugStream->print(value, BIN);
    }
  #endif
}

// ═══════════════════════════════════════════════════════════════════
// LEVELED DEBUG MACROS
// ═══════════════════════════════════════════════════════════════════

#if DEBUG_ENABLED

  // Basic macros
  #define DEBUG_PRINT(msg)       debug_print(msg)
  #define DEBUG_PRINTLN(msg)     debug_println(msg)
  #define DEBUG_PRINTF(...)      debug_printf(__VA_ARGS__)
  #define DEBUG_PRINTFLN(...)    debug_printfln(__VA_ARGS__)
  #define DEBUG_HEX(val)         debug_printHex(val)
  #define DEBUG_BIN(val)         debug_printBin(val)
  #define DEBUG_NEWLINE()        debug_newline()

  // Leveled macros
  #if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
    #define DEBUG_ERROR(msg)     do { debug_print("[ERROR] "); debug_println(msg); } while(0)
    #define DEBUG_ERRORF(...)    do { debug_print("[ERROR] "); debug_printfln(__VA_ARGS__); } while(0)
  #else
    #define DEBUG_ERROR(msg)
    #define DEBUG_ERRORF(...)
  #endif

  #if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
    #define DEBUG_WARN(msg)      do { debug_print("[WARN] "); debug_println(msg); } while(0)
    #define DEBUG_WARNF(...)     do { debug_print("[WARN] "); debug_printfln(__VA_ARGS__); } while(0)
  #else
    #define DEBUG_WARN(msg)
    #define DEBUG_WARNF(...)
  #endif

  #if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
    #define DEBUG_INFO(msg)      do { debug_print("[INFO] "); debug_println(msg); } while(0)
    #define DEBUG_INFOF(...)     do { debug_print("[INFO] "); debug_printfln(__VA_ARGS__); } while(0)
  #else
    #define DEBUG_INFO(msg)
    #define DEBUG_INFOF(...)
  #endif

  #if DEBUG_LEVEL >= DEBUG_LEVEL_VERBOSE
    #define DEBUG_VERBOSE(msg)   do { debug_print("[VERBOSE] "); debug_println(msg); } while(0)
    #define DEBUG_VERBOSEF(...)  do { debug_print("[VERBOSE] "); debug_printfln(__VA_ARGS__); } while(0)
  #else
    #define DEBUG_VERBOSE(msg)
    #define DEBUG_VERBOSEF(...)
  #endif

#else // DEBUG_ENABLED == 0

  // All debug macros become no-ops
  #define DEBUG_PRINT(msg)
  #define DEBUG_PRINTLN(msg)
  #define DEBUG_PRINTF(...)
  #define DEBUG_PRINTFLN(...)
  #define DEBUG_HEX(val)
  #define DEBUG_BIN(val)
  #define DEBUG_NEWLINE()

  #define DEBUG_ERROR(msg)
  #define DEBUG_ERRORF(...)
  #define DEBUG_WARN(msg)
  #define DEBUG_WARNF(...)
  #define DEBUG_INFO(msg)
  #define DEBUG_INFOF(...)
  #define DEBUG_VERBOSE(msg)
  #define DEBUG_VERBOSEF(...)

#endif // DEBUG_ENABLED

// ═══════════════════════════════════════════════════════════════════
// TIMING DEBUG HELPERS
// ═══════════════════════════════════════════════════════════════════

#if DEBUG_ENABLED

/**
 * Print timestamp with message
 */
void debug_timestamp(const char* msg) {
  if (!g_debugStream) return;

  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;

  g_debugStream->print(F("["));
  if (min < 10) g_debugStream->print("0");
  g_debugStream->print(min);
  g_debugStream->print(F(":"));
  if ((sec % 60) < 10) g_debugStream->print("0");
  g_debugStream->print(sec % 60);
  g_debugStream->print(F("."));
  if ((ms % 1000) < 100) g_debugStream->print("0");
  if ((ms % 1000) < 10) g_debugStream->print("0");
  g_debugStream->print(ms % 1000);
  g_debugStream->print(F("] "));
  g_debugStream->println(msg);
}

#define DEBUG_TIMESTAMP(msg) debug_timestamp(msg)

#else
  #define DEBUG_TIMESTAMP(msg)
#endif

// ═══════════════════════════════════════════════════════════════════
// DEBUG SECTION HELPERS
// ═══════════════════════════════════════════════════════════════════

#if DEBUG_ENABLED

/**
 * Print section header
 */
void debug_section(const char* title) {
  if (!g_debugStream) return;

  g_debugStream->println();
  g_debugStream->println(F("-------------------------------------------------------"));
  g_debugStream->print(F("  ")); g_debugStream->println(title);
  g_debugStream->println(F("-------------------------------------------------------"));
}

/**
 * Print boxed header
 */
void debug_box(const char* title) {
  if (!g_debugStream) return;

  g_debugStream->println();
  g_debugStream->println(F("======================================================="));
  g_debugStream->print(F("         ")); g_debugStream->println(title);
  g_debugStream->println(F("======================================================="));
}

#define DEBUG_SECTION(title) debug_section(title)
#define DEBUG_BOX(title)     debug_box(title)

#else
  #define DEBUG_SECTION(title)
  #define DEBUG_BOX(title)
#endif

// ═══════════════════════════════════════════════════════════════════
// DEBUG INFO
// ═══════════════════════════════════════════════════════════════════

/**
 * Print debug HAL information
 */
void debug_printInfo() {
  #if DEBUG_ENABLED
    if (!g_debugStream) return;

    g_debugStream->println();
    g_debugStream->println(F("======================================================="));
    g_debugStream->println(F("         DEBUG HAL INFORMATION                        "));
    g_debugStream->println(F("======================================================="));
    g_debugStream->print(F("Interface:    ")); g_debugStream->println(debug_getInterfaceName());
    g_debugStream->print(F("Initialized:  ")); g_debugStream->println(g_debugInitialized ? "Yes" : "No");
    g_debugStream->print(F("TFT on UART:  ")); g_debugStream->println(g_tftUsingUart ? "Yes" : "No");
    g_debugStream->print(F("Debug Level:  ")); g_debugStream->println(DEBUG_LEVEL);
    g_debugStream->print(F("Baudrate:     ")); g_debugStream->println(DEBUG_BAUDRATE);
    g_debugStream->println(F("=======================================================\n"));
  #endif
}

#endif // DEBUG_HAL_H
