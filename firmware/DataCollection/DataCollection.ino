// ================================================================
// DataCollection.ino
// Arduino Nano 33 IoT | LDR (A0) + NTC Thermistor (A1)
// ================================================================
// PURPOSE:
//   Streams sensor readings over USB serial continuously.
//   Python script (capture_data.py) decides when to save rows.
//
// OUTPUT FORMAT (one row per 200ms):
//   timestamp_ms,LightMean,TempMean
//
// STARTUP:
//   Runs a hardware check before streaming begins.
//   Prints HW_OK or HW_FAIL so Python can abort early
//   if a sensor is not connected or wired incorrectly.
//
// HARDWARE CHECK THRESHOLDS:
//   LDR raw ADC  : must be between 50 and 950
//     - 0    = LDR not connected or short circuit
//     - 1023 = wire disconnected (floating pin reads high)
//   Temp (C)     : must be between 0 and 60
//     - <0 or >60 = thermistor not connected or wrong resistor
//
// NOTES:
//   - No while(!Serial) — board works without laptop connected
//   - LEDs forced OFF during collection — prevents LED light
//     from affecting LDR readings
//   - Each LightMean and TempMean is the average of 8 rapid
//     burst reads, not a single ADC read — cleaner values
// ================================================================

#include <math.h>

// ── Pins ──────────────────────────────────────────────────────
const int LDR_PIN        = A0;
const int THERMISTOR_PIN = A1;
const int LED1           = 2;
const int LED2           = 3;
const int LED3           = 4;

// ── Thermistor constants (NTC 10k) ────────────────────────────
const float SERIES_RESISTOR    = 10000.0f;
const float NOMINAL_RESISTANCE = 10000.0f;
const float NOMINAL_TEMP       = 25.0f;
const float B_COEFFICIENT      = 3950.0f;

// ── Timing ────────────────────────────────────────────────────
const int SAMPLE_RATE_MS = 200;   // one row every 200ms = 5 per second

// ── Burst sampling ────────────────────────────────────────────
const int BURST_COUNT    = 8;     // reads averaged per sample
const int BURST_DELAY_US = 500;   // microseconds between burst reads

// ── Hardware check thresholds ─────────────────────────────────
const float LDR_MIN_ADC  = 50.0f;
const float LDR_MAX_ADC  = 950.0f;
const float TEMP_MIN_C   = 0.0f;
const float TEMP_MAX_C   = 60.0f;
const int   HW_CHECK_READS = 10;   // averaged readings for check

// ── State ─────────────────────────────────────────────────────
bool headerPrinted = false;

// ================================================================
// FORWARD DECLARATIONS
// ================================================================
float burstReadLDR();
float burstReadTemp();
void  ledsOff();
void  ledsOn();
void  flashLEDs(int times, int onMs, int offMs);

// ================================================================
// SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  // NO while(!Serial) -- board works standalone without USB

  // Force all LEDs OFF -- LED light affects LDR readings
  pinMode(LED1, OUTPUT); digitalWrite(LED1, LOW);
  pinMode(LED2, OUTPUT); digitalWrite(LED2, LOW);
  pinMode(LED3, OUTPUT); digitalWrite(LED3, LOW);

  // Discard first 10 ADC reads -- lets ADC reference stabilise
  // after power-on before any real readings are taken
  for (int i = 0; i < 10; i++) {
    analogRead(LDR_PIN);
    analogRead(THERMISTOR_PIN);
    delay(10);
  }

  // ── Hardware check ──────────────────────────────────────────
  // Take HW_CHECK_READS readings from each sensor and average them.
  // A single read can be noisy -- averaging gives a reliable result.
  float ldrSum  = 0.0f;
  float tempSum = 0.0f;

  for (int i = 0; i < HW_CHECK_READS; i++) {
    ldrSum  += (float)analogRead(LDR_PIN);
    tempSum += burstReadTemp();
    delay(50);
  }

  float ldrAvg  = ldrSum  / HW_CHECK_READS;
  float tempAvg = tempSum / HW_CHECK_READS;

  bool ldrOk  = (ldrAvg  >= LDR_MIN_ADC && ldrAvg  <= LDR_MAX_ADC);
  bool tempOk = (tempAvg >= TEMP_MIN_C   && tempAvg <= TEMP_MAX_C);

  if (ldrOk && tempOk) {
    // Both sensors responding normally
    Serial.print("# HW_OK: LDR=");
    Serial.print(ldrAvg, 0);
    Serial.print("  Temp=");
    Serial.print(tempAvg, 1);
    Serial.println("C");

    // Two slow flashes = hardware OK
    flashLEDs(2, 250, 250);

  } else {
    // One or both sensors failed -- report each failure separately
    if (!ldrOk) {
      Serial.print("# HW_FAIL: LDR=");
      Serial.print(ldrAvg, 0);
      if (ldrAvg < LDR_MIN_ADC) {
        Serial.println("  -- reading too low, check LDR wiring on A0");
      } else {
        Serial.println("  -- reading too high, check LDR wiring on A0");
      }
    }

    if (!tempOk) {
      Serial.print("# HW_FAIL: Temp=");
      Serial.print(tempAvg, 1);
      Serial.println("C  -- check thermistor wiring on A1");
    }

    // Rapid blink signals hardware failure -- visible without laptop
    // Board stays in this loop until repowered or reflashed
    while (true) {
      ledsOn();  delay(100);
      ledsOff(); delay(100);
    }
  }
}

// ================================================================
// MAIN LOOP
// ================================================================
void loop() {
  // Print CSV header on the very first row
  if (!headerPrinted) {
    Serial.println("timestamp_ms,LightMean,TempMean");
    headerPrinted = true;
  }

  // Read both sensors (each is the average of 8 burst reads)
  float lightMean = burstReadLDR();
  float tempMean  = burstReadTemp();

  // Sanity check on temperature -- skip and warn if implausible
  // This catches occasional electrical glitches mid-session
  if (tempMean < -40.0f || tempMean > 120.0f) {
    Serial.println("# WARNING: Implausible temp reading -- row skipped");
    delay(SAMPLE_RATE_MS);
    return;
  }

  // Send one CSV row
  Serial.print(millis());
  Serial.print(",");
  Serial.print(lightMean, 2);
  Serial.print(",");
  Serial.println(tempMean, 3);

  delay(SAMPLE_RATE_MS);
}

// ================================================================
// BURST READ -- LDR (raw ADC, 0-1023)
// Returns the average of BURST_COUNT rapid reads.
// ================================================================
float burstReadLDR() {
  float sum = 0.0f;
  for (int i = 0; i < BURST_COUNT; i++) {
    sum += (float)analogRead(LDR_PIN);
    delayMicroseconds(BURST_DELAY_US);
  }
  return sum / BURST_COUNT;
}

// ================================================================
// BURST READ -- Thermistor (converts ADC to Celsius)
// Returns the average of BURST_COUNT rapid reads in degrees C.
// ADC clamped to 1-1022 to prevent division by zero in the
// Steinhart-Hart equation if pin floats to 0 or 1023.
// ================================================================
float burstReadTemp() {
  float sum = 0.0f;

  for (int i = 0; i < BURST_COUNT; i++) {
    int rawADC = analogRead(THERMISTOR_PIN);

    // Clamp -- avoids division by zero in Steinhart-Hart
    if (rawADC < 1)    rawADC = 1;
    if (rawADC > 1022) rawADC = 1022;

    float resistance = SERIES_RESISTOR / (1023.0f / (float)rawADC - 1.0f);
    float steinhart  = resistance / NOMINAL_RESISTANCE;
    steinhart        = log(steinhart);
    steinhart       /= B_COEFFICIENT;
    steinhart       += 1.0f / (NOMINAL_TEMP + 273.15f);
    steinhart        = 1.0f / steinhart;
    steinhart       -= 273.15f;

    sum += steinhart;
    delayMicroseconds(BURST_DELAY_US);
  }

  return sum / BURST_COUNT;
}

// ================================================================
// LED HELPERS
// ================================================================
void ledsOff() {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
}

void ledsOn() {
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, HIGH);
}

void flashLEDs(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    ledsOn();  delay(onMs);
    ledsOff(); delay(offMs);
  }
}
