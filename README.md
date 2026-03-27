# IoT Anomaly Detection System

## Overview
An end-to-end embedded anomaly detection system built on Arduino Nano 33 IoT.
The system monitors ambient light and temperature continuously using an LDR and
NTC thermistor, and flags sudden unexpected changes as anomalies using a K-Means
model deployed directly on the microcontroller.

## Hardware
| Component | Connection |
|---|---|
| Arduino Nano 33 IoT | -- |
| LDR (light dependent resistor) | A0 |
| NTC 10k thermistor | A1 |
| LED 1 | D2 |
| LED 2 | D3 |
| LED 3 | D4 |

## How It Works
1. Arduino streams LightMean and TempMean over USB serial at 200ms intervals
2. Python script captures sessions as timestamped CSV files
3. CSV files uploaded to Edge Impulse and K-Means model trained on normal data only
4. Trained model deployed back onto Arduino as a TinyML library
5. On-device inference runs standalone -- LEDs blink slow (normal) or fast (anomaly)

## Why K-Means Instead of a Neural Network
An earlier version of this project used a supervised neural network classifier
trained in Edge Impulse. It failed in deployment because training data was collected
in a single short session under fixed lighting, so the model had no knowledge of
normal variation across different times of day and lighting conditions. Any change
in ambient light was flagged as anomaly.

K-Means trained on normal data only solves this by building a map of what normal
looks like across diverse conditions. Anything that falls far outside that map is
flagged as anomaly -- no anomaly examples needed during training.

## Data Collection Strategy
- 4 minutes active recording per session
- Sessions spread across morning, day, evening, night
- Conditions: indoor artificial, indoor near window, outdoor sun, outdoor overcast
- ~1200 rows per session at 200ms sample interval
- Each session saved as a separate CSV named by condition and timestamp

## Repository Structure
```
iot-anomaly-detector/
├── firmware/DataCollection/    -- Arduino sketch for data collection
├── deployment/                 -- Arduino sketch for inference (after training)
├── data-capture/               -- Python serial capture script
├── data/                       -- CSV format description (no raw data files)
├── hardware/                   -- Circuit notes and pin connections
└── docs/                       -- Session log and project notes
```

## Project Status
- [x] Hardware assembled and tested
- [x] Data collection firmware written
- [x] Python capture script written
- [ ] Data collection in progress (target: 10+ sessions)
- [ ] Edge Impulse K-Means training
- [ ] Threshold tuning and testing
- [ ] Final deployment

## Tools and Technologies
- Arduino Nano 33 IoT (SAMD21 ARM Cortex-M0+)
- Edge Impulse (K-Means anomaly detection block)
- Python / pyserial (data capture)
- C/C++ (embedded firmware)
- TinyML (on-device inference)
