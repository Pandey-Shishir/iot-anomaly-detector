# Circuit Notes

## Components
- Arduino Nano 33 IoT
- LDR (any standard 5mm LDR, ~10k in normal light)
- NTC thermistor 10k at 25C (B coefficient 3950)
- 10k resistor in series with thermistor (voltage divider)
- 3x LEDs with appropriate current limiting resistors on D2, D3, D4

## Pin Connections
| Pin | Component | Notes |
|---|---|---|
| A0 | LDR | One leg to 3.3V, other leg to A0 and 10k to GND |
| A1 | Thermistor | Voltage divider with 10k series resistor to 3.3V |
| D2 | LED 1 | With current limiting resistor to GND |
| D3 | LED 2 | With current limiting resistor to GND |
| D4 | LED 3 | With current limiting resistor to GND |

## Notes
- LEDs are forced OFF during data collection to prevent light contamination on LDR
- Thermistor uses Steinhart-Hart equation (B parameter method) for ADC to Celsius conversion
- Series resistor value must match SERIES_RESISTOR constant in firmware (default 10000)

## Known Compatibility Issue
Edge Impulse Arduino library (v1.0.1) requires a manual fix for
Arduino SAMD boards (Cortex-M0+). In the installed library file:

  src/edge-impulse-sdk/CMSIS/DSP/Include/arm_math_types.h

The condition `#elif defined (__GNUC_PYTHON__)` must be changed to
`#elif defined (__GNUC_PYTHON__) || defined (__GNUC__)`
and a software `__SSAT` macro added for Cortex-M0+ compatibility.
This must be re-applied each time the library is reinstalled.
