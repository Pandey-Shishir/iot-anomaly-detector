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

### Step 1 -- Upload firmware to Arduino
Open `firmware/DataCollection/DataCollection.ino` in Arduino IDE and upload
it to the Arduino Nano 33 IoT. Once uploaded, the Arduino immediately starts
streaming sensor readings over USB serial. Close Arduino IDE and Serial Monitor
completely before moving to Step 2.

### Step 2 -- Run the Python capture script
With the Arduino still connected via USB, run `data-capture/capture_data.py`
on your laptop. The script connects to the Arduino over serial, verifies both
sensors are responding, then asks you to describe the session condition before
starting a 4 minute recording.

These two must run together -- the Arduino streams data continuously over USB
and the Python script receives and saves it. The Arduino does not need to be
re-uploaded between sessions, only between code changes.

### Step 3 -- Train on Edge Impulse
Upload collected CSV files to Edge Impulse using the CSV Wizard. Train a
K-Means anomaly detection model on normal data only.

### Step 4 -- Deploy
Export the trained model from Edge Impulse as an Arduino library, then upload
`deployment/SmartAnomalyDetector/Smart_Anomaly_Detector.ino` to the Arduino.
The board then runs standalone -- no laptop needed.

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

## Edge Impulse Pipeline

- Platform: Edge Impulse (edgeimpulse.com)
- Data uploaded via CSV Wizard as time series data
- Sample frequency: 5Hz (200ms intervals)
- Window size: 2000ms (10 readings per window)
- Processing block: Flatten (Average, Minimum, Maximum, Standard Deviation)
- Learning block: Anomaly Detection (K-Means)
- Clusters: 8
- Training samples: 541 windows
- Test samples: 129 windows
- All data labelled as: normal

## Deployment

- Model exported from Edge Impulse as Arduino library
- Library installed via Arduino IDE
- Inference sketch uploaded to Arduino Nano 33 IoT
- Anomaly threshold tuned by observing Serial Monitor scores
- Final threshold: [fill in your value]
- Board runs fully standalone after deployment -- no laptop needed

## Anomaly Detection Logic

K-Means builds 8 cluster centres from normal training data.
At inference time, the model extracts 6 features from each
2-second window and calculates the distance to the nearest
cluster centre. That distance is the anomaly score.

- Score below threshold -- NORMAL -- LEDs blink slowly
- Score above threshold -- ANOMALY -- LEDs blink fast

Tested anomaly conditions:
- Covering LDR with hand (sudden darkness)
- Holding warm object near thermistor (sudden heat)
- Switching room lights off suddenly


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

## Tools and Technologies
- Arduino Nano 33 IoT (SAMD21 ARM Cortex-M0+)
- Edge Impulse (K-Means anomaly detection block)
- Python / pyserial (data capture)
- C/C++ (embedded firmware)
- TinyML (on-device inference)
