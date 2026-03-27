#define __STATIC_FORCEINLINE __attribute__((always_inline)) static inline

#include <iot-anomaly-detector_inferencing.h>

/* ============================================================
 *  Smart Anomaly Detector -- Arduino Nano 33 IoT
 * ============================================================
 *  Sensors : LDR on A0 | NTC Thermistor on A1
 *  LEDs    : 3x LED on D2, D3, D4
 *
 *  LED Behaviour:
 *    Startup        -- blinks 3 times slowly to confirm power on
 *    NORMAL         -- blinks slowly every 500ms
 *    ANOMALY        -- blinks rapidly every 100ms
 *    Config error   -- rapid blink, halts permanently
 *
 *  Runs fully standalone -- no laptop or Serial Monitor needed.
 *  If laptop is connected, anomaly score and status print to
 *  Serial Monitor at 115200 baud for threshold tuning.
 *
 * SETUP:
 *  1. Install the Edge Impulse library zip via:
 *     Arduino IDE -> Sketch -> Include Library -> Add .ZIP Library
 *  2. Update the #include line below to match your exported
 *     library name exactly (check Arduino IDE -> Examples menu)
 *  3. Upload this sketch to Arduino Nano 33 IoT
 *  4. Open Serial Monitor at 115200 baud to see anomaly scores
 *  5. Note normal scores and anomaly scores, then set
 *     ANOMALY_THRESHOLD between the two values
 *
 * ANOMALY THRESHOLD TUNING:
 *  - Start with threshold 0.5 (default below)
 *  - Observe Serial Monitor scores in normal conditions
 *  - Cover LDR or hold warm object near thermistor
 *  - Note the score difference
 *  - Set threshold halfway between normal and anomaly scores
 *  - Re-upload with updated threshold value
 *
 * COMPATIBILITY NOTE:
 *  The #define __STATIC_FORCEINLINE at the top is required for
 *  Arduino Nano 33 IoT (Cortex-M0+) compatibility with the
 *  Edge Impulse CMSIS library. Do not remove it.
 *  Additionally, arm_math_types.h in the installed library
 *  requires a manual fix -- see hardware/circuit_notes.md
 *  in the GitHub repository for details.
 * ============================================================ */

/* ── Pin Definitions ─────────────────────────────────────── */
#define LDR_PIN             A0
#define THERM_PIN           A1
#define LED1_PIN            2
#define LED2_PIN            3
#define LED3_PIN            4

/* ── LED Blink Timing ────────────────────────────────────── */
#define BLINK_SLOW          500     // ms -- normal state
#define BLINK_FAST          100     // ms -- anomaly state

/* ── Anomaly Threshold ───────────────────────────────────── */
// Tune this after observing Serial Monitor scores.
// Increase if normal conditions trigger false anomalies.
// Decrease if real anomalies are not being detected.
#define ANOMALY_THRESHOLD   0.5f

/* ── Thermistor Constants (NTC 10k) ──────────────────────── */
#define SERIES_RESISTOR     10000.0f
#define NOMINAL_RESISTANCE  10000.0f
#define NOMINAL_TEMP        25.0f
#define B_COEFFICIENT       3950.0f

/* ── Debug -- set true to print raw sensor values ────────── */
static const bool DEBUG_SENSORS = false;

/* ── State ───────────────────────────────────────────────── */
static bool          is_anomaly      = false;
static unsigned long last_blink_time = 0;
static bool          led_state       = false;

/* ── Forward Declarations ────────────────────────────────── */
static float read_ldr(void);
static float read_thermistor(void);
static void  set_leds(bool state);
static void  blink_leds(int interval_ms);

/* ==========================================================
 *  SETUP
 * ========================================================== */
void setup()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);

    // Three slow blinks -- confirms board powered on
    for (int i = 0; i < 3; i++) {
        set_leds(true);  delay(200);
        set_leds(false); delay(200);
    }

    // Serial is optional -- only active if laptop connected
    // Does NOT block or halt if no laptop is present
    Serial.begin(115200);
    delay(500);

    Serial.println("========================================");
    Serial.println("  Smart Anomaly Detector");
    Serial.println("  Arduino Nano 33 IoT");
    Serial.println("  LDR (A0) + Thermistor NTC (A1)");
    Serial.println("  Standalone mode -- no laptop needed");
    Serial.println("========================================");
    Serial.print  ("  Anomaly threshold : ");
    Serial.println(ANOMALY_THRESHOLD, 2);
    Serial.println("========================================\n");

    // Verify model expects exactly 2 input features (LDR + Temp)
    // If this fails, the wrong library was installed
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 2) {
        Serial.print("ERR: Model expects ");
        Serial.print(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
        Serial.println(" features per frame but sketch provides 2.");
        Serial.println("Check library matches this sketch.");
        // Rapid blink permanently -- visible without Serial Monitor
        while (true) {
            set_leds(true);  delay(100);
            set_leds(false); delay(100);
        }
    }

    Serial.println("Model loaded OK. Starting inference...\n");
}

/* ==========================================================
 *  MAIN LOOP
 * ========================================================== */
void loop()
{
    // Blink LEDs at current rate while collecting samples
    int blink_interval = is_anomaly ? BLINK_FAST : BLINK_SLOW;
    blink_leds(blink_interval);

    // ── Fill sensor buffer ────────────────────────────────
    // Collects 2 seconds of data at 200ms intervals (10 readings).
    // Buffer layout: [LDR_0, Temp_0, LDR_1, Temp_1, ... LDR_9, Temp_9]
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
         ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
    {
        // Schedule next read precisely to maintain 200ms interval
        int64_t next_tick = (int64_t)micros() +
                            ((int64_t)EI_CLASSIFIER_INTERVAL_MS * 1000);

        // Read both sensors
        // IMPORTANT: read_ldr() returns raw ADC (0-1023) to match
        // training data which was also raw ADC -- not normalised
        buffer[ix + 0] = read_ldr();
        buffer[ix + 1] = read_thermistor();

        if (DEBUG_SENSORS) {
            Serial.print("  LDR: ");
            Serial.print(buffer[ix + 0], 1);
            Serial.print("  Temp: ");
            Serial.println(buffer[ix + 1], 2);
        }

        // Keep blinking LEDs during sample collection
        blink_leds(blink_interval);

        // Wait until next scheduled tick
        int64_t wait_time = next_tick - (int64_t)micros();
        if (wait_time > 0) delayMicroseconds((uint32_t)wait_time);
    }

    // ── Convert buffer to Edge Impulse signal ────────────
    signal_t signal;
    int err = numpy::signal_from_buffer(
                  buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        Serial.print("ERR: signal_from_buffer failed (");
        Serial.print(err);
        Serial.println(")");
        return;
    }

    // ── Run inference ─────────────────────────────────────
    ei_impulse_result_t result = { 0 };
    err = run_classifier(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        Serial.print("ERR: run_classifier failed (");
        Serial.print(err);
        Serial.println(")");
        return;
    }

    // ── Read anomaly score and update LED state ───────────
#if EI_CLASSIFIER_HAS_ANOMALY == 1

    float anomaly_score = result.anomaly;

    Serial.println("----------------------------------------");
    Serial.print  ("  LDR raw    : "); Serial.println(analogRead(LDR_PIN));
    Serial.print  ("  Temp (C)   : "); Serial.println(read_thermistor(), 2);
    Serial.print  ("  Score      : "); Serial.println(anomaly_score, 4);
    Serial.print  ("  Threshold  : "); Serial.println(ANOMALY_THRESHOLD, 2);

    if (anomaly_score > ANOMALY_THRESHOLD) {
        is_anomaly = true;
        Serial.println("  STATUS     : ** ANOMALY ** -- LEDs fast");
    } else {
        is_anomaly = false;
        Serial.println("  STATUS     : NORMAL -- LEDs slow");
    }

    Serial.println("----------------------------------------\n");

#else
    // Model was exported without anomaly block -- should not happen
    // if K-Means was set up correctly in Edge Impulse
    Serial.println("ERR: Model has no anomaly detection block.");
    Serial.println("Re-check Edge Impulse impulse design.");
#endif
}

/* ==========================================================
 *  SENSOR FUNCTIONS
 * ========================================================== */

// Returns raw ADC value (0-1023) -- matches training data format
static float read_ldr(void)
{
    return (float)analogRead(LDR_PIN);
}

// Returns temperature in Celsius using Steinhart-Hart B parameter method
// ADC clamped to 1-1022 to prevent division by zero
static float read_thermistor(void)
{
    int rawADC = analogRead(THERM_PIN);

    // Clamp to avoid division by zero in Steinhart-Hart
    if (rawADC < 1)    rawADC = 1;
    if (rawADC > 1022) rawADC = 1022;

    float resistance = SERIES_RESISTOR / (1023.0f / (float)rawADC - 1.0f);
    float steinhart  = resistance / NOMINAL_RESISTANCE;
    steinhart        = log(steinhart);
    steinhart       /= B_COEFFICIENT;
    steinhart       += 1.0f / (NOMINAL_TEMP + 273.15f);
    steinhart        = 1.0f / steinhart;
    return steinhart - 273.15f;
}

/* ==========================================================
 *  LED FUNCTIONS
 * ========================================================== */

static void set_leds(bool state)
{
    digitalWrite(LED1_PIN, state ? HIGH : LOW);
    digitalWrite(LED2_PIN, state ? HIGH : LOW);
    digitalWrite(LED3_PIN, state ? HIGH : LOW);
}

// Non-blocking blink -- checks elapsed time and toggles if due
static void blink_leds(int interval_ms)
{
    unsigned long now = millis();
    if (now - last_blink_time >= (unsigned long)interval_ms) {
        last_blink_time = now;
        led_state = !led_state;
        set_leds(led_state);
    }
}
