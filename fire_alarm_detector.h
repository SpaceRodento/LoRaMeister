/*=====================================================================
  fire_alarm_detector.h - Unified Fire Alarm Detection

  Detects fire alarms via audio (MAX4466) and/or light (LM393).
  Combines ENABLE_AUDIO_DETECTION and ENABLE_LIGHT_DETECTION features.

  AUDIO DETECTION:
  - Monitors sound patterns (85dB @ 3kHz, 3-4 beeps/second)
  - Hardware: MAX4466 microphone amplifier on GPIO 34 (ADC1_CH6)

  LIGHT DETECTION: ✅ LM393 (2026-02-05)
  - Detects smoke detector flash patterns (3 flashes = alarm)
  - Hardware: LM393 Light Sensor Switch Module
  - Dual-mode: Digital (DO) + Analog (AO)
  - GPIO power management: VCC/GND (platform-specific)

  API:
  - initFireAlarmDetector(): Initialize enabled detectors
  - checkFireAlarm(): Update detection status
  - isFireAlarmActive(): Check if any alarm is active
  - isAudioAlarmActive(): Check audio-only alarm
  - isLightAlarmActive(): Check light-only alarm
  - getFireAlarmStatus(): Get detailed alarm state
=======================================================================*/

#ifndef FIRE_ALARM_DETECTOR_H
#define FIRE_ALARM_DETECTOR_H

#include <Arduino.h>
#include "config.h"

#if ENABLE_LIGHT_DETECTION
// LM393 Light Sensor Module Driver (inlined from lm393_light_detector.h)

// ═══════════════════════════════════════════════════════════════════
// LM393 SENSOR STATE
// ═══════════════════════════════════════════════════════════════════

struct LM393State {
  // Current readings
  int adcValue;                  // Raw ADC reading (0-4095)
  float voltage;                 // Voltage (0-3.3V)
  bool digitalState;             // Digital pin state (HIGH/LOW)
  bool isLightDetected;          // Bright light detected (analog)
  bool isDark;                   // Dark condition (analog)
  int brightness;                // Brightness percentage (0-100)

  // Flash detection state
  unsigned long lastFlashTime;   // Timestamp of last flash
  unsigned long flashOnTime;     // Timestamp when flash started
  unsigned long flashOffTime;    // Timestamp when flash ended
  int flashCount;                // Number of flashes detected
  bool alarmActive;              // Alarm state

  // Adaptive update rate
  bool highPriorityMode;         // High-speed mode (100Hz vs 20Hz)
  unsigned long modeChangeTime;  // When mode was last changed
  bool forceHighPriority;        // Single trigger variable (set by various methods)

  // Scheduled window state
  unsigned long lastScheduledCheck;    // Last scheduled window start time
  bool scheduledWindowActive;          // Currently in scheduled window

  // Delta detection state
  int previousADC;                     // Previous ADC value for delta
  unsigned long deltaWindowStart;      // Start of delta detection window
  bool deltaTriggered;                 // Delta threshold exceeded

  // DO pin trigger state
  bool previousDigitalState;           // Previous DO pin state
  bool doTriggered;                    // DO pin changed

  // Adaptive baseline state
  int baselineADC;                     // Learned ambient ADC level
  unsigned long baselineStartTime;     // When baseline learning started
  bool baselineInitialized;            // Baseline learning complete
  unsigned long deviationStartTime;    // When sustained deviation began (for re-learning)

  // Statistics
  unsigned long samplesProcessed;
  unsigned long lastUpdate;
  bool sensorAvailable;                // Sensor connected (stability test at init, or baseline learning)
};

static LM393State lm393 = {0};

// ═══════════════════════════════════════════════════════════════════
// ADAPTIVE UPDATE RATE
// ═══════════════════════════════════════════════════════════════════
// Two-speed update system for efficient flash detection:
//
// LOW PRIORITY (20Hz, 50ms):  Normal monitoring, saves CPU
// HIGH PRIORITY (100Hz, 10ms): Fast flash detection, triggered by activity
//
// Automatically switches between modes based on sensor activity.
// See updateLM393() for trigger logic.
//
// Note: LM393_HIGH_PRIORITY_TIMEOUT is defined in config.h

#define LM393_UPDATE_NORMAL 50      // 50ms = 20Hz (normal mode)
#define LM393_UPDATE_FAST 10        // 10ms = 100Hz (high priority mode)

// ═══════════════════════════════════════════════════════════════════
// SENSOR READING
// ═══════════════════════════════════════════════════════════════════

/**
 * Read LM393 sensor (analog + digital)
 * Note: Analog output is INVERTED (bright = low ADC, dark = high ADC)
 */
inline void readLM393Sensor() {
  // Read analog value (inverted: bright = low, dark = high)
  lm393.adcValue = analogRead(LIGHT_ANALOG_PIN);
  lm393.voltage = (lm393.adcValue / 4095.0) * 3.3;

  // Read digital state (depends on potentiometer setting)
  lm393.digitalState = digitalRead(LIGHT_PIN);

  // Calculate brightness (invert ADC: 0 = bright, 4095 = dark)
  lm393.brightness = map(lm393.adcValue, 4095, 0, 0, 100);
  lm393.brightness = constrain(lm393.brightness, 0, 100);

  // Detect light conditions
  // When DO trigger is enabled, use DO pin (more sensitive, potentiometer-adjustable)
  // DO pin: LOW = light detected, HIGH = dark
  #if LM393_ENABLE_DO_TRIGGER
    lm393.isLightDetected = !lm393.digitalState;  // DO LOW = light
    lm393.isDark = lm393.digitalState;             // DO HIGH = dark
  #else
    // Fallback: use analog thresholds (inverted: low ADC = bright)
    lm393.isLightDetected = (lm393.adcValue < LM393_BRIGHT_THRESHOLD);
    lm393.isDark = (lm393.adcValue > LM393_DARK_THRESHOLD);
  #endif

  lm393.samplesProcessed++;
}

// ═══════════════════════════════════════════════════════════════════
// FLASH PATTERN DETECTION
// ═══════════════════════════════════════════════════════════════════

/**
 * Detect flash pattern (3 flashes = smoke detector alarm)
 * Returns true if alarm confirmed
 *
 * Flash source selection:
 * - When LM393_ENABLE_DO_TRIGGER: Uses DO pin (hardware threshold via potentiometer)
 *   More sensitive, adjustable with blue potentiometer on LM393 module
 * - Otherwise: Uses analog ADC threshold (LM393_BRIGHT_THRESHOLD)
 *   Less sensitive in normal room lighting conditions
 *
 * DO pin logic: LOW = light detected, HIGH = dark
 */
inline bool detectLM393FlashPattern() {
  unsigned long now = millis();

  // Determine if light is detected using the best available method
  // DO pin (potentiometer-adjustable) is preferred when enabled,
  // because analog values may not be sensitive enough in room lighting
  #if LM393_ENABLE_DO_TRIGGER
    bool lightOn = !lm393.digitalState;  // DO: LOW = light, HIGH = dark
  #else
    bool lightOn = lm393.isLightDetected;  // Analog: ADC < BRIGHT_THRESHOLD
  #endif

  // Light detected (flash ON)
  if (lightOn) {
    if (lm393.flashOnTime == 0) {
      lm393.flashOnTime = now;

      #if DEBUG_STATE_MACHINE
      Serial.print("💡 LM393 Flash ON (");
      #if LM393_ENABLE_DO_TRIGGER
      Serial.print("DO=LOW");
      #else
      Serial.print("ADC=");
      Serial.print(lm393.adcValue);
      #endif
      Serial.println(")");
      #endif

      if (lm393.lastFlashTime > 0) {
        unsigned long interval = now - lm393.lastFlashTime;

        if (interval >= LIGHT_FLASH_INTERVAL_MIN && interval <= LIGHT_FLASH_INTERVAL_MAX) {
          lm393.flashCount++;

          #if DEBUG_STATE_MACHINE
          Serial.print("💡 LM393 Flash detected! Count: ");
          Serial.print(lm393.flashCount);
          Serial.print(", Interval: ");
          Serial.print(interval);
          Serial.println(" ms");
          #endif

          if (lm393.flashCount >= LIGHT_FLASH_CONFIRM_COUNT) {
            lm393.alarmActive = true;

            Serial.println();
            Serial.println("╔════════════════════════════════════════╗");
            Serial.println("║  🚨 SMOKE DETECTOR ALARM DETECTED! 🚨  ║");
            Serial.println("╚════════════════════════════════════════╝");
            Serial.print("  Flash count: ");
            Serial.println(lm393.flashCount);
            Serial.println("  Pattern: Red LED strobe (630nm)");
            Serial.println("  Source: LM393 light sensor");
            Serial.println();

            return true;
          }
        } else {
          #if DEBUG_STATE_MACHINE
          Serial.print("⚠️  LM393 Invalid flash interval: ");
          Serial.print(interval);
          Serial.print(" ms (expected ");
          Serial.print(LIGHT_FLASH_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(LIGHT_FLASH_INTERVAL_MAX);
          Serial.println("ms)");
          #endif
          lm393.flashCount = 1;
        }
      } else {
        lm393.flashCount = 1;
      }
    }
  }
  // No light (flash OFF)
  else {
    if (lm393.flashOnTime > 0) {
      lm393.flashOffTime = now;
      unsigned long flashDuration = lm393.flashOffTime - lm393.flashOnTime;

      if (flashDuration >= LIGHT_FLASH_MIN_MS && flashDuration <= LIGHT_FLASH_MAX_MS) {
        lm393.lastFlashTime = lm393.flashOffTime;

        #if DEBUG_STATE_MACHINE
        Serial.print("  Flash OFF, duration: ");
        Serial.print(flashDuration);
        Serial.println(" ms ✓");
        #endif
      } else {
        #if DEBUG_STATE_MACHINE
        Serial.print("⚠️  LM393 Invalid flash duration: ");
        Serial.print(flashDuration);
        Serial.print(" ms (expected ");
        Serial.print(LIGHT_FLASH_MIN_MS);
        Serial.print("-");
        Serial.print(LIGHT_FLASH_MAX_MS);
        Serial.println("ms)");
        #endif
        lm393.flashCount = 0;
        lm393.lastFlashTime = 0;
      }

      lm393.flashOnTime = 0;
    }

    // Timeout reset: no new flash within the allowed interval window
    // Uses configured max interval + 50% margin instead of hardcoded value
    unsigned long flashTimeout = LIGHT_FLASH_INTERVAL_MAX + (LIGHT_FLASH_INTERVAL_MAX / 2);
    if (lm393.lastFlashTime > 0 && (now - lm393.lastFlashTime) > flashTimeout) {
      #if DEBUG_STATE_MACHINE
      if (lm393.flashCount > 0) {
        Serial.print("⚠️  LM393 Flash sequence timeout (");
        Serial.print(flashTimeout);
        Serial.println("ms), resetting");
      }
      #endif
      lm393.flashCount = 0;
      lm393.lastFlashTime = 0;
    }
  }

  return false;
}

// ═══════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════

/**
 * Initialize LM393 light sensor
 */
inline void initLM393() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  LM393 LIGHT SENSOR INIT               ║");
  Serial.println("╚════════════════════════════════════════╝");

  // Initialize struct
  memset(&lm393, 0, sizeof(LM393State));
  lm393.lastUpdate = millis();

  // Scheduled window: trigger first window immediately at startup
  // Without this, lastScheduledCheck=0 would delay first window by interval hours
  #if LM393_ENABLE_SCHEDULED_WINDOW
  lm393.scheduledWindowActive = true;
  lm393.lastScheduledCheck = millis();
  #endif

  // Configure analog pin
  pinMode(LIGHT_ANALOG_PIN, INPUT);
  analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
  analogReadResolution(12);         // 12-bit resolution (0-4095)

  Serial.print("✓ Analog pin (GPIO ");
  Serial.print(LIGHT_ANALOG_PIN);
  Serial.println("): INPUT (ADC)");

  // Configure digital pin
  pinMode(LIGHT_PIN, INPUT);
  Serial.print("✓ Digital pin (GPIO ");
  Serial.print(LIGHT_PIN);
  Serial.println("): INPUT");

  Serial.println("✓ ADC resolution: 12-bit (0-4095)");

  // Read initial values for diagnostics
  const int NUM_SAMPLES = 10;
  int samples[NUM_SAMPLES];
  int sum = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(LIGHT_ANALOG_PIN);
    sum += samples[i];
    delay(5);
  }

  int initialAnalog = samples[NUM_SAMPLES - 1];
  int initialDigital = digitalRead(LIGHT_PIN);
  int avgValue = sum / NUM_SAMPLES;

  // Calculate max deviation from average
  int maxDeviation = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int dev = abs(samples[i] - avgValue);
    if (dev > maxDeviation) maxDeviation = dev;
  }

  Serial.print("Initial analog reading: ");
  Serial.print(initialAnalog);
  Serial.print(" (");
  Serial.print((initialAnalog / 4095.0) * 3.3, 2);
  Serial.println(" V)");

  Serial.print("Initial digital reading: ");
  Serial.println(initialDigital == HIGH ? "HIGH" : "LOW");

  Serial.print("Startup readings: avg=");
  Serial.print(avgValue);
  Serial.print(", max deviation=");
  Serial.println(maxDeviation);

  // Sensor auto-detection (can be disabled via LM393_AUTO_DETECT_SENSOR in config.h)
  #if LM393_AUTO_DETECT_SENSOR
  {
    // Pullup/pulldown test on the DIGITAL pin (LIGHT_PIN / DO)
    // LM393 modules have ~10kΩ onboard pullup on DO, and the comparator
    // output is low-impedance when active. These override ESP32's 45kΩ pulls:
    //
    //   Sensor BRIGHT: comparator pulls LOW (<100Ω) → PU=LOW,  PD=LOW  (driven)
    //   Sensor DARK:   module 10kΩ pullup to VCC    → PU=HIGH, PD=HIGH (driven)
    //   No sensor:     pin floats, follows 45kΩ     → PU=HIGH, PD=LOW  (floating)
    //
    // Note: AO pin is high-impedance (voltage divider) and cannot be tested
    //       this way - the internal pullup overrides the weak analog signal.

    #if USE_XIAO_SX1262
    // XIAO GPIO 4 supports INPUT_PULLUP/INPUT_PULLDOWN
    pinMode(LIGHT_PIN, INPUT_PULLUP);
    delay(10);
    bool doWithPullup = digitalRead(LIGHT_PIN);

    pinMode(LIGHT_PIN, INPUT_PULLDOWN);
    delay(10);
    bool doWithPulldown = digitalRead(LIGHT_PIN);

    // Restore normal input
    pinMode(LIGHT_PIN, INPUT);

    bool pinFloating = (doWithPullup == HIGH && doWithPulldown == LOW);

    Serial.print("DO pin pullup/pulldown test: PU=");
    Serial.print(doWithPullup ? "HIGH" : "LOW");
    Serial.print(", PD=");
    Serial.print(doWithPulldown ? "HIGH" : "LOW");
    Serial.print(" → ");
    Serial.println(pinFloating ? "FLOATING" : "DRIVEN");

    if (pinFloating) {
      Serial.println("⚠️  AUTO-DETECT: DO pin floating - no sensor connected");
      lm393.sensorAvailable = false;
    } else {
      Serial.println("✓ AUTO-DETECT: Sensor connected (DO pin driven)");
      lm393.sensorAvailable = true;
    }

    #else
    // ESP32 WROOM: GPIO 34 is input-only, no internal pull resistors
    // Fallback: DO pin stability test (floating pins fluctuate)
    const int DO_SAMPLES = 100;
    int doHighCount = 0;
    for (int i = 0; i < DO_SAMPLES; i++) {
      if (digitalRead(LIGHT_PIN) == HIGH) doHighCount++;
      delay(5);  // 500ms total
    }

    // A connected sensor is stable (all HIGH in dark, all LOW in bright)
    // A floating pin fluctuates between HIGH and LOW
    bool doStable = (doHighCount <= 5 || doHighCount >= 95);

    Serial.print("DO pin stability test: ");
    Serial.print(doHighCount);
    Serial.print("/");
    Serial.print(DO_SAMPLES);
    Serial.print(" HIGH → ");
    Serial.println(doStable ? "STABLE" : "UNSTABLE");

    if (!doStable) {
      Serial.println("⚠️  AUTO-DETECT: DO pin unstable - no sensor connected");
      lm393.sensorAvailable = false;
    } else {
      Serial.println("✓ AUTO-DETECT: Sensor connected (DO pin stable)");
      lm393.sensorAvailable = true;
    }
    #endif
  }
  #else
    // Auto-detection disabled: assume sensor is always connected
    lm393.sensorAvailable = true;
    Serial.println("✓ Sensor detection: MANUAL (always available)");
  #endif

  if (!lm393.sensorAvailable) {
    Serial.println();
    Serial.println("🚫 LM393 light sensor DISABLED (no sensor detected)");
    Serial.println("   LIT:0 (N/A) will be sent in payloads");
  } else {
    // Print configuration only when sensor is active
    Serial.println();
    Serial.println("Configuration:");
    Serial.print("  Bright threshold: ");
    Serial.print(LM393_BRIGHT_THRESHOLD);
    Serial.println(" ADC (inverted: low = bright)");
    Serial.print("  Dark threshold:   ");
    Serial.print(LM393_DARK_THRESHOLD);
    Serial.println(" ADC (inverted: high = dark)");
    Serial.print("  Flash duration:   ");
    Serial.print(LIGHT_FLASH_MIN_MS);
    Serial.print("-");
    Serial.print(LIGHT_FLASH_MAX_MS);
    Serial.println(" ms");
    Serial.print("  Flash interval:   ");
    Serial.print(LIGHT_FLASH_INTERVAL_MIN);
    Serial.print("-");
    Serial.print(LIGHT_FLASH_INTERVAL_MAX);
    Serial.println(" ms");
    Serial.print("  Confirm count:    ");
    Serial.println(LIGHT_FLASH_CONFIRM_COUNT);
    Serial.println();
    Serial.println("💡 LM393 light sensor ready");
  }
  Serial.println();
}

// ═══════════════════════════════════════════════════════════════════
// HIGH PRIORITY TRIGGER SYSTEM
// ═══════════════════════════════════════════════════════════════════

/**
 * Check all enabled trigger conditions and set forceHighPriority
 * This is the central control point for high-speed mode activation
 *
 * TRIGGER METHODS (independently enabled via config.h):
 *   1. Debug mode (always 100Hz when DEBUG_LIGHT_SENSOR enabled)
 *   2. Scheduled window (periodic monitoring, e.g., 2min every hour)
 *   3. DO pin trigger (hardware threshold change)
 *   4. Delta detection (ADC brightness change)
 *   5. Flash sequence tracking (active flash counting)
 *   6. Hybrid filter (DO + AO validation)
 *   7. Adaptive baseline (learned ambient deviation)
 */
inline void checkHighPriorityTriggers() {
  unsigned long now = millis();
  lm393.forceHighPriority = false;  // Reset, then check each trigger

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 1: DEBUG MODE - Always 100Hz when debugging
  // ═══════════════════════════════════════════════════════════════════
  #if DEBUG_LIGHT_SENSOR && LM393_DEBUG_FORCE_HIGH_PRIORITY
  lm393.forceHighPriority = true;
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 2: SCHEDULED WINDOW - Periodic high-speed monitoring
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_SCHEDULED_WINDOW
  {
    unsigned long intervalMs = (unsigned long)LM393_SCHEDULE_INTERVAL_HOURS * 3600UL * 1000UL;
    unsigned long durationMs = (unsigned long)LM393_SCHEDULE_DURATION_SEC * 1000UL;

    // Check if window is currently active FIRST (prevents re-trigger)
    if (lm393.scheduledWindowActive) {
      if (now - lm393.lastScheduledCheck < durationMs) {
        lm393.forceHighPriority = true;
      } else {
        lm393.scheduledWindowActive = false;
        #if DEBUG_STATE_MACHINE
        Serial.println("🕐 LM393: Scheduled window ended");
        #endif
      }
    }
    // Only check for new window when not already active
    else if (now - lm393.lastScheduledCheck >= intervalMs) {
      lm393.lastScheduledCheck = now;
      lm393.scheduledWindowActive = true;

      #if DEBUG_STATE_MACHINE
      Serial.println("🕐 LM393: Starting scheduled monitoring window");
      Serial.print("  Duration: ");
      Serial.print(LM393_SCHEDULE_DURATION_SEC);
      Serial.println(" seconds");
      #endif
    }
  }
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 3: DO PIN TRIGGER - Hardware threshold detection
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_DO_TRIGGER
  {
    if (lm393.digitalState != lm393.previousDigitalState) {
      lm393.doTriggered = true;
      lm393.forceHighPriority = true;

      #if DEBUG_STATE_MACHINE
      Serial.print("💡 LM393: DO pin trigger (");
      Serial.print(lm393.digitalState ? "HIGH" : "LOW");
      Serial.println(")");
      #endif
    }

    // Keep high priority for timeout period after trigger
    if (lm393.doTriggered) {
      lm393.forceHighPriority = true;
    }

    lm393.previousDigitalState = lm393.digitalState;
  }
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 4: DELTA DETECTION - ADC brightness change
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_DELTA_DETECTION
  {
    int delta = abs(lm393.adcValue - lm393.previousADC);

    if (delta > LM393_DELTA_THRESHOLD) {
      lm393.deltaTriggered = true;
      lm393.deltaWindowStart = now;
      lm393.forceHighPriority = true;

      #if DEBUG_STATE_MACHINE
      Serial.print("💡 LM393: Delta trigger (change: ");
      Serial.print(delta);
      Serial.println(" ADC)");
      #endif
    }

    // Keep high priority for window duration after delta
    if (lm393.deltaTriggered) {
      if (now - lm393.deltaWindowStart < LM393_DELTA_WINDOW_MS) {
        lm393.forceHighPriority = true;
      } else {
        lm393.deltaTriggered = false;
      }
    }

    lm393.previousADC = lm393.adcValue;
  }
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 5: FLASH SEQUENCE TRACKING - Active flash counting
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_FLASH_SEQUENCE
  {
    if (lm393.flashCount > 0) {
      lm393.forceHighPriority = true;
    }
  }
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 6: HYBRID FILTER - DO gate + AO validation
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_HYBRID_FILTER
  {
    // DO pin acts as gate (detects light change), AO confirms intensity
    // DO=LOW means light detected by comparator, then analog validates
    // This reduces false positives: both channels must agree
    if (!lm393.digitalState &&
        lm393.adcValue < LM393_HYBRID_CONFIRM_THRESHOLD) {
      lm393.forceHighPriority = true;

      #if DEBUG_STATE_MACHINE
      static unsigned long lastHybridDebug = 0;
      if (now - lastHybridDebug > 1000) {
        Serial.print("💡 LM393: Hybrid confirmed (DO=LOW + ADC=");
        Serial.print(lm393.adcValue);
        Serial.print(" < ");
        Serial.print(LM393_HYBRID_CONFIRM_THRESHOLD);
        Serial.println(")");
        lastHybridDebug = now;
      }
      #endif
    }
  }
  #endif

  // ═══════════════════════════════════════════════════════════════════
  // TRIGGER 7: ADAPTIVE BASELINE - Learn and detect deviations
  // ═══════════════════════════════════════════════════════════════════
  #if LM393_ENABLE_ADAPTIVE_BASELINE
  {
    unsigned long baselineWindowMs = (unsigned long)LM393_BASELINE_WINDOW_SEC * 1000UL;

    // Initialize baseline learning
    if (!lm393.baselineInitialized) {
      if (lm393.baselineStartTime == 0) {
        lm393.baselineStartTime = now;
        lm393.baselineADC = lm393.adcValue;

        #if DEBUG_STATE_MACHINE
        Serial.print("💡 LM393: Starting baseline learning (");
        Serial.print(LM393_BASELINE_WINDOW_SEC);
        Serial.println(" seconds)");
        #endif
      }

      // Update baseline with moving average during learning
      if (now - lm393.baselineStartTime < baselineWindowMs) {
        // Simple moving average
        lm393.baselineADC = (lm393.baselineADC * 9 + lm393.adcValue) / 10;
      } else {
        lm393.baselineInitialized = true;

        Serial.println("╔════════════════════════════════════════╗");
        Serial.print(  "║  💡 LM393 Baseline learned: ");
        Serial.print(lm393.baselineADC);
        Serial.println(" ADC     ║");
        Serial.print(  "║  Deviation threshold: ");
        Serial.print(LM393_BASELINE_DEVIATION);
        Serial.println(" ADC          ║");
        Serial.println("╚════════════════════════════════════════╝");
      }
    }

    // Check for deviations from baseline
    if (lm393.baselineInitialized) {
      int deviation = abs(lm393.adcValue - lm393.baselineADC);

      if (deviation > LM393_BASELINE_DEVIATION) {
        lm393.forceHighPriority = true;

        // Track how long deviation has been sustained
        if (lm393.deviationStartTime == 0) {
          lm393.deviationStartTime = now;
        }

        // If deviation persists for 30+ seconds without flashes,
        // the environment changed (lamp on, daylight shift) - re-learn
        if (lm393.flashCount == 0 &&
            (now - lm393.deviationStartTime) > 30000UL) {
          int oldBaseline = lm393.baselineADC;
          lm393.baselineADC = lm393.adcValue;
          lm393.deviationStartTime = 0;

          #if DEBUG_STATE_MACHINE
          Serial.print("💡 LM393: Baseline re-learned: ");
          Serial.print(oldBaseline);
          Serial.print(" → ");
          Serial.print(lm393.baselineADC);
          Serial.println(" ADC (sustained change, no flashes)");
          #endif
        }

        #if DEBUG_STATE_MACHINE
        static unsigned long lastBaselineDebug = 0;
        if (now - lastBaselineDebug > 1000) {
          Serial.print("💡 LM393: Baseline deviation (");
          Serial.print(deviation);
          Serial.print(" ADC, baseline=");
          Serial.print(lm393.baselineADC);
          Serial.println(")");
          lastBaselineDebug = now;
        }
        #endif
      } else {
        lm393.deviationStartTime = 0;  // Reset sustained deviation timer

        // Slowly adapt baseline to current conditions when stable
        // (no flash activity, no large deviations)
        // Weight: 99/100 old + 1/100 new → drifts ~1% per reading
        lm393.baselineADC = (lm393.baselineADC * 99 + lm393.adcValue) / 100;
      }
    }
  }
  #endif
}

/**
 * Update LM393 sensor readings and detect flashes
 * Uses adaptive update rate: 100Hz when active, 20Hz when idle
 * Call this in main loop()
 */
inline void updateLM393() {
  // Startup test determined no sensor connected - skip all processing
  if (!lm393.sensorAvailable) return;

  unsigned long now = millis();

  // ═══════════════════════════════════════════════════════════════════
  // ADAPTIVE UPDATE RATE LOGIC
  // ═══════════════════════════════════════════════════════════════════
  // Uses modular trigger system (see checkHighPriorityTriggers() above)
  // Single control variable: forceHighPriority
  //
  // HIGH PRIORITY (100Hz, 10ms): Fast flash detection
  // LOW PRIORITY (20Hz, 50ms): Battery-efficient monitoring
  // ═══════════════════════════════════════════════════════════════════

  // Use appropriate update interval based on current mode
  unsigned long updateInterval = lm393.highPriorityMode ? LM393_UPDATE_FAST : LM393_UPDATE_NORMAL;

  // Throttle updates based on current mode
  if (now - lm393.lastUpdate < updateInterval) {
    return;
  }

  // Read sensor to get fresh values
  readLM393Sensor();

  // Check all enabled trigger conditions (sets forceHighPriority)
  checkHighPriorityTriggers();

  // Timeout handling: Reset individual triggers after inactivity
  if (lm393.highPriorityMode &&
      !lm393.forceHighPriority &&
      (now - lm393.modeChangeTime >= LM393_HIGH_PRIORITY_TIMEOUT)) {
    // Reset all trigger states
    lm393.doTriggered = false;
    lm393.deltaTriggered = false;
  }

  // Determine mode based on forceHighPriority
  bool shouldBeHighPriority = lm393.forceHighPriority;

  // Also keep high priority for timeout period after last trigger
  if (lm393.highPriorityMode && (now - lm393.modeChangeTime < LM393_HIGH_PRIORITY_TIMEOUT)) {
    shouldBeHighPriority = true;
  }

  // Switch modes if needed (affects next update interval)
  if (shouldBeHighPriority && !lm393.highPriorityMode) {
    lm393.highPriorityMode = true;
    lm393.modeChangeTime = now;
    #if DEBUG_STATE_MACHINE
    Serial.println("💡 LM393: Switching to HIGH PRIORITY mode (100Hz)");
    #endif
  } else if (!shouldBeHighPriority && lm393.highPriorityMode) {
    lm393.highPriorityMode = false;
    lm393.modeChangeTime = now;
    #if DEBUG_STATE_MACHINE
    Serial.println("💡 LM393: Switching to LOW PRIORITY mode (20Hz)");
    #endif
  }

  // Detect flash pattern
  detectLM393FlashPattern();

  lm393.lastUpdate = now;
}

/**
 * Check if LM393 alarm is active
 */
inline bool isLM393AlarmActive() {
  return lm393.alarmActive;
}

/**
 * Get current LM393 sensor state (for LCD display, telemetry, etc.)
 */
inline LM393State getLM393State() {
  return lm393;
}

/**
 * Reset LM393 alarm state (for testing)
 */
inline void resetLM393Alarm() {
  lm393.alarmActive = false;
  lm393.flashCount = 0;
  lm393.lastFlashTime = 0;
  lm393.flashOnTime = 0;
  lm393.flashOffTime = 0;
}


#endif

// ═══════════════════════════════════════════════════════════════════
// AUDIO DETECTION (from audio_detector.h)
// ═══════════════════════════════════════════════════════════════════

#if ENABLE_AUDIO_DETECTION

// Audio detection configuration
#ifndef AUDIO_PIN
  #define AUDIO_PIN 34                  // ADC1_CH6
#endif
#define AUDIO_SAMPLE_WINDOW 50
#ifndef AUDIO_SAMPLES
  #define AUDIO_SAMPLES 50
#endif
#ifndef AUDIO_THRESHOLD
  #define AUDIO_THRESHOLD 200
#endif
#define AUDIO_SUSTAINED_MS 1000    // Require sustained sound for this long (ms)
#ifndef AUDIO_PEAK_MIN
  #define AUDIO_PEAK_MIN 2         // Match config.h default
#endif
#ifndef AUDIO_PEAK_MAX
  #define AUDIO_PEAK_MAX 6         // Match config.h default
#endif
#ifndef AUDIO_COOLDOWN
  #define AUDIO_COOLDOWN 5000
#endif

int audioThreshold = AUDIO_THRESHOLD;

struct AudioDetector {
  int currentRMS;
  int peakCount;
  unsigned long lastPeakTime;
  bool alarmDetected;
  unsigned long alarmStartTime;
  unsigned long lastAlertTime;
  int alertCount;
  int baselineRMS;
  int maxRMS;
  bool isCalibrated;
  unsigned long samplesProcessed;
  int falsePositives;
  unsigned long lastUpdate;
};

AudioDetector audio = {0, 0, 0, false, 0, 0, 0, 0, 0, false, 0, 0, 0};

inline int calculateRMS() {
  unsigned long sum = 0;
  int samples = AUDIO_SAMPLES;

  for (int i = 0; i < samples; i++) {
    int sample = analogRead(AUDIO_PIN);
    // 12-bit ADC: 0-4095, center at 2048 (DC offset)
    // Cast to long before squaring to prevent overflow
    // Max amplitude = 2048, max amplitude^2 = 4,194,304 (fits in unsigned long)
    long amplitude = (long)sample - 2048;
    sum += (unsigned long)(amplitude * amplitude);
    delayMicroseconds((AUDIO_SAMPLE_WINDOW * 1000) / samples);
  }

  int rms = (int)sqrt((double)sum / samples);
  audio.samplesProcessed += samples;
  return rms;
}

inline bool detectPeak(int rms) {
  unsigned long now = millis();

  if (rms > audioThreshold) {
    if (now - audio.lastPeakTime > 100) {
      audio.peakCount++;
      audio.lastPeakTime = now;
      return true;
    }
  }

  if (now - audio.lastPeakTime > 1000) {
    audio.peakCount = 0;
  }

  return false;
}

inline bool isAlarmPattern() {
  return (audio.peakCount >= AUDIO_PEAK_MIN && audio.peakCount <= AUDIO_PEAK_MAX);
}

inline void updateAudioDetection() {
  unsigned long now = millis();
  audio.currentRMS = calculateRMS();

  if (audio.currentRMS > audio.maxRMS) {
    audio.maxRMS = audio.currentRMS;
  }

  bool peakDetected = detectPeak(audio.currentRMS);
  bool highVolume = (audio.currentRMS > audioThreshold);

  if (highVolume && !audio.alarmDetected) {
    if (audio.alarmStartTime == 0) {
      audio.alarmStartTime = now;
    }

    if (now - audio.alarmStartTime >= AUDIO_SUSTAINED_MS) {
      if (isAlarmPattern()) {
        audio.alarmDetected = true;
        audio.alertCount++;
        audio.lastAlertTime = now;
      } else {
        audio.falsePositives++;
        audio.alarmStartTime = 0;
      }
    }
  }
  else if (!highVolume && audio.alarmDetected) {
    audio.alarmDetected = false;
    audio.alarmStartTime = 0;
  }
  else if (!highVolume) {
    audio.alarmStartTime = 0;
  }

  audio.lastUpdate = now;
}

inline void initAudioDetector() {
  pinMode(AUDIO_PIN, INPUT);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  audio.lastUpdate = millis();
  Serial.println("🔊 Audio detection initialized (GPIO 34)");
}

inline bool isAudioAlarmActive() {
  return audio.alarmDetected;
}

#endif // ENABLE_AUDIO_DETECTION

// ═══════════════════════════════════════════════════════════════════
// UNIFIED FIRE ALARM DETECTOR
// ═══════════════════════════════════════════════════════════════════

struct FireAlarmState {
  bool audioAlarmActive;
  bool lightAlarmActive;
  unsigned long lastAlertTime;
  unsigned long alertCount;
  unsigned long audioDetections;
  unsigned long lightDetections;
  unsigned long combinedDetections;
};

static FireAlarmState fireAlarmState = {false, false, 0, 0, 0, 0, 0};

inline void initFireAlarmDetector() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  FIRE ALARM DETECTOR INIT              ║");
  Serial.println("╚════════════════════════════════════════╝");

  #if ENABLE_AUDIO_DETECTION
  Serial.println("  Initializing AUDIO detector...");
  initAudioDetector();
  #endif

  #if ENABLE_LIGHT_DETECTION
  Serial.println("  Initializing LM393 LIGHT detector...");
  initLM393();
  #endif

  #if !ENABLE_AUDIO_DETECTION && !ENABLE_LIGHT_DETECTION
  Serial.println("  ⚠️  NO DETECTORS ENABLED!");
  #endif

  Serial.println("Fire alarm detector ready.\n");
}

inline void checkFireAlarm() {
  bool audioTriggered = false;
  bool lightTriggered = false;

  #if ENABLE_AUDIO_DETECTION
  updateAudioDetection();
  audioTriggered = isAudioAlarmActive();
  fireAlarmState.audioAlarmActive = audioTriggered;
  #endif

  #if ENABLE_LIGHT_DETECTION
  updateLM393();
  lightTriggered = isLM393AlarmActive();
  fireAlarmState.lightAlarmActive = lightTriggered;
  #endif

  // Combine triggers
  if (audioTriggered || lightTriggered) {
    unsigned long now = millis();

    #if ENABLE_AUDIO_DETECTION
    if (now - fireAlarmState.lastAlertTime > AUDIO_COOLDOWN) {
    #else
    if (now - fireAlarmState.lastAlertTime > 5000) {  // 5s cooldown default
    #endif
      fireAlarmState.lastAlertTime = now;
      fireAlarmState.alertCount++;

      if (audioTriggered) fireAlarmState.audioDetections++;
      if (lightTriggered) fireAlarmState.lightDetections++;
      if (audioTriggered && lightTriggered) {
        fireAlarmState.combinedDetections++;
      }

      Serial.println("\n🚨 FIRE ALARM DETECTED!");

      #if ENABLE_AUDIO_DETECTION && ENABLE_LIGHT_DETECTION
      if (audioTriggered && lightTriggered) {
        Serial.println("  Method: AUDIO + LIGHT (CONFIRMED)");
      } else
      #endif
      #if ENABLE_AUDIO_DETECTION
      if (audioTriggered) {
        Serial.println("  Method: AUDIO ONLY");
      }
      #endif
      #if ENABLE_LIGHT_DETECTION
      if (lightTriggered) {
        Serial.println("  Method: LM393 LIGHT");
      }
      #endif
    }
  }
}

inline bool isFireAlarmActive() {
  return fireAlarmState.audioAlarmActive || fireAlarmState.lightAlarmActive;
}

inline String getFireAlarmStatus() {
  #if ENABLE_AUDIO_DETECTION && ENABLE_LIGHT_DETECTION
  if (fireAlarmState.audioAlarmActive && fireAlarmState.lightAlarmActive) {
    return "COMBINED_ALARM";
  }
  #endif
  #if ENABLE_AUDIO_DETECTION
  if (fireAlarmState.audioAlarmActive) {
    return "AUDIO_ALARM";
  }
  #endif
  #if ENABLE_LIGHT_DETECTION
  if (fireAlarmState.lightAlarmActive) {
    return "LIGHT_ALARM";
  }
  #endif
  return "IDLE";
}

// Accessors for compatibility with other modules
#if ENABLE_LIGHT_DETECTION
inline LM393State getLM393StateForDisplay() {
  return getLM393State();
}

inline bool isLightAlarmActive() {
  return isLM393AlarmActive();
}

// Debug output for LM393 sensor (test/calibration mode)
// Call this periodically (e.g., every 1 second) when DEBUG_LIGHT_SENSOR is enabled
inline void printLM393DebugInfo() {
  LM393State lm393 = getLM393State();
  if (!lm393.sensorAvailable) return;

  // Format: ADC:2048 | Voltage:1.65V | Digital:LOW | Brightness:50% | State:AMBIENT | Flashes:0
  Serial.print("ADC:");
  Serial.print(lm393.adcValue);
  Serial.print(" | Voltage:");
  Serial.print(lm393.voltage, 2);
  Serial.print("V | Digital:");
  Serial.print(lm393.digitalState ? "HIGH" : "LOW ");
  Serial.print(" | Brightness:");
  Serial.print(lm393.brightness);
  Serial.print("% | State:");

  if (lm393.isLightDetected) {
    Serial.print("BRIGHT");
  } else if (lm393.isDark) {
    Serial.print("DARK  ");
  } else {
    Serial.print("AMBIENT");
  }

  Serial.print(" | Flashes:");
  Serial.println(lm393.flashCount);
}
#endif

#endif // FIRE_ALARM_DETECTOR_H
