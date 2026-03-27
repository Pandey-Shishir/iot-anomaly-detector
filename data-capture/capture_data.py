"""
capture_data.py
===============
Captures sensor data from Arduino Nano 33 IoT for exactly 4 minutes
of ACTIVE recording time and saves it as a CSV file.

USAGE:
    python capture_data.py

REQUIREMENTS:
    pip install pyserial

HOW IT WORKS:
    1. Connects to Arduino over USB serial
    2. Reads a few rows to verify both sensors are responding
       -- exits immediately if values are implausible
    3. Asks three questions to describe the session condition
    4. Starts logging when you confirm
    5. Records exactly 4 minutes of active data
       -- pause with  p + Enter  (clock stops)
       -- resume with r + Enter  (clock continues)
    6. Saves CSV to the same folder as this script

OUTPUT FILE:
    Named from your answers, e.g.:
      morning_indoor_sunny_20260326_1430.csv

CSV FORMAT:
    timestamp_ms,LightMean,TempMean

NOTES:
    - Change PORT below to match your board
    - Check: Device Manager -> Ports (COM & LPT) -> Arduino Nano 33 IoT
    - Arduino IDE and Serial Monitor must be fully closed before running
"""

import serial
import serial.tools.list_ports
import time
import os
import sys
import threading
from datetime import datetime

# ================================================================
# CONFIG
# ================================================================
PORT             = "COM5"
BAUD             = 115200
ACTIVE_DURATION  = 4 * 60        # 4 minutes in seconds

CSV_HEADER = "timestamp_ms,LightMean,TempMean\n"

# ================================================================
# SHARED STATE (used between main thread and input thread)
# ================================================================
command           = None
command_lock      = threading.Lock()
input_thread_stop = threading.Event()

# ================================================================
# INPUT THREAD
# Runs in background, reads keyboard commands without blocking
# the main serial-reading loop.
# ================================================================
def input_listener():
    while not input_thread_stop.is_set():
        try:
            line = input().strip().lower()
        except EOFError:
            break
        except Exception:
            break

        global command
        if line in ('p', 'r'):
            with command_lock:
                command = line

# ================================================================
# HELPERS
# ================================================================
def list_com_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("  No COM ports found. Is the board plugged in?")
    else:
        print("  Available COM ports:")
        for p in ports:
            print(f"    {p.device}  --  {p.description}")
    print()


def ask_choice(question, options):
    """
    Asks a numbered question and returns the chosen option string.
    Keeps asking until a valid number is entered.
    """
    print(f"\n  {question}")
    for i, opt in enumerate(options, 1):
        print(f"    {i}. {opt}")
    while True:
        answer = input("  Enter number: ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            chosen = options[int(answer) - 1]
            print(f"  -> {chosen}")
            return chosen
        print(f"  Please enter a number between 1 and {len(options)}.")


def build_filename(time_of_day, location, weather):
    """
    Builds a CSV filename from session conditions and current time.
    Example: morning_indoor_sunny_20260326_1430.csv
    """
    timestamp = datetime.now().strftime("%Y%m%d_%H%M")
    parts = [
        time_of_day.lower().replace(" ", ""),
        location.lower().replace(" ", ""),
        weather.lower().replace(" ", ""),
        timestamp
    ]
    return "_".join(parts) + ".csv"


def format_time(seconds):
    """Formats seconds as MM:SS string."""
    m = int(seconds) // 60
    s = int(seconds) % 60
    return f"{m:02d}:{s:02d}"

# ================================================================
# SENSOR CHECK
# Reads incoming rows from Arduino and validates that both
# sensors are producing plausible values.
# Does not depend on any special startup message -- works by
# reading the continuous CSV stream that Arduino always sends.
# ================================================================
def wait_for_hw_check(ser):
    print()
    print("  Checking sensors -- waiting for data...")
    deadline = time.time() + 15

    while time.time() < deadline:
        try:
            raw = ser.readline()
        except serial.SerialException:
            return False

        if not raw:
            continue

        try:
            line = raw.decode("utf-8").strip()
        except UnicodeDecodeError:
            continue

        if not line:
            continue

        # Skip header and comment lines
        if line.startswith("#") or "timestamp_ms" in line:
            continue

        # Check if it is a valid 3-column data row
        parts = line.split(",")
        if len(parts) == 3:
            try:
                float(parts[0])          # timestamp_ms
                ldr  = float(parts[1])   # LightMean
                temp = float(parts[2])   # TempMean

                # Sanity check temperature
                if temp < -40 or temp > 120:
                    print(f"  WARNING: Implausible temp reading: {temp:.1f}C")
                    print(f"  Check thermistor wiring on A1.")
                    return False

                # Sanity check LDR (0-1023 is full ADC range)
                if ldr < 1 or ldr > 1022:
                    print(f"  WARNING: Implausible LDR reading: {ldr:.0f}")
                    print(f"  Check LDR wiring on A0.")
                    return False

                # Values are plausible -- sensors are working
                print(f"  Sensors OK -- LDR={ldr:.0f}  Temp={temp:.1f}C")
                return True

            except ValueError:
                continue

    print("  No data received within timeout.")
    print("  Check that DataCollection.ino is uploaded to the board.")
    return False

# ================================================================
# MAIN
# ================================================================
def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print()
    print("=" * 57)
    print("   IoT Anomaly Detection -- Data Capture")
    print("   Arduino Nano 33 IoT | LDR + Thermistor")
    print("=" * 57)
    print(f"   Port        : {PORT}")
    print(f"   Record time : 4 minutes active")
    print(f"   Save folder : {script_dir}")
    print("=" * 57)

    # Show available COM ports
    print()
    list_com_ports()

    # Check the configured port exists
    available = [p.device for p in serial.tools.list_ports.comports()]
    if PORT not in available:
        print(f"  ERROR: {PORT} not found.")
        print(f"  Open capture_data.py and change the PORT line near the top.")
        print(f"  Available ports: {available if available else 'none'}")
        print()
        input("  Press ENTER to exit.")
        sys.exit(1)

    input(f"  Press ENTER when board is connected and Arduino IDE is closed...")

    # ── Connect ──────────────────────────────────────────────────
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"\n  ERROR: Could not open {PORT}: {e}")
        print()
        print("  Checklist:")
        print("    1. Board is plugged in via USB")
        print("    2. Arduino IDE is fully closed")
        print("    3. Serial Monitor tab is closed")
        print("    4. PORT in this script matches Device Manager")
        print()
        input("  Press ENTER to exit.")
        sys.exit(1)

    # Arduino resets automatically when serial port opens.
    # No sleep here -- start reading immediately so we don't
    # miss any startup output from the board.
    print()
    print("  Connected. Reading startup output...")

    # ── Sensor check ─────────────────────────────────────────────
    hw_ok = wait_for_hw_check(ser)

    if not hw_ok:
        print()
        print("  !! SENSOR CHECK FAILED !!")
        print()
        print("  Sensors are not responding with valid values.")
        print("  Check wiring and re-run the script.")
        print()
        ser.close()
        input("  Press ENTER to exit.")
        sys.exit(1)

    print()
    print("  Both sensors are responding correctly.")

    # ── Flush rows that arrived while user reads the screen ───────
    ser.reset_input_buffer()

    # ── Session condition questions ──────────────────────────────
    print()
    print("  Answer three questions to describe this session.")
    print("  These are used to name the saved file.")

    time_of_day = ask_choice(
        "Time of day:",
        ["morning", "day", "evening", "night"]
    )

    location = ask_choice(
        "Location:",
        ["indoor", "outdoor"]
    )

    weather = ask_choice(
        "Light condition:",
        ["sunny", "cloudy", "artificial"]
    )

    # ── Build filename ────────────────────────────────────────────
    csv_filename = build_filename(time_of_day, location, weather)
    csv_path     = os.path.join(script_dir, csv_filename)

    # Warn if a file with this name already exists
    if os.path.exists(csv_path):
        print()
        print(f"  WARNING: {csv_filename} already exists in this folder.")
        overwrite = input("  Overwrite? (yes to continue, anything else to cancel): ").strip().lower()
        if overwrite != "yes":
            print("  Cancelled. Re-run the script to choose again.")
            ser.close()
            sys.exit(0)

    # ── Ready prompt ─────────────────────────────────────────────
    print()
    print(f"  Session  : {time_of_day} / {location} / {weather}")
    print(f"  File     : {csv_filename}")
    print(f"  Duration : 4 minutes active recording")
    print()
    print("  During recording:")
    print("    p + Enter  ->  pause   (clock stops)")
    print("    r + Enter  ->  resume  (clock continues)")
    print("    Ctrl+C     ->  abort   (partial file saved)")
    print()

    go = input("  Ready to start? (yes to begin): ").strip().lower()
    if go != "yes":
        print("  Cancelled.")
        ser.close()
        sys.exit(0)

    # Flush anything buffered while user was answering questions
    ser.reset_input_buffer()

    # ── Start input listener thread ───────────────────────────────
    t = threading.Thread(target=input_listener, daemon=True)
    t.start()

    # ── Recording loop ────────────────────────────────────────────
    rows_written   = 0
    active_seconds = 0.0
    paused         = False
    last_tick      = time.time()
    done           = False

    print()
    print("  Recording started.")
    print()

    try:
        with open(csv_path, "w", newline="", encoding="utf-8") as csv_file:

            csv_file.write(CSV_HEADER)
            csv_file.flush()

            while not done:

                # ── Check for pause / resume command ─────────────
                with command_lock:
                    cmd = command
                    globals()['command'] = None

                if cmd == 'p' and not paused:
                    paused = True
                    print(f"  [PAUSED at {format_time(active_seconds)} -- type r to resume]")

                elif cmd == 'r' and paused:
                    paused = False
                    last_tick = time.time()
                    print(f"  [RESUMED -- {format_time(ACTIVE_DURATION - active_seconds)} remaining]")

                # ── Advance active timer when not paused ──────────
                if not paused:
                    now = time.time()
                    active_seconds += now - last_tick
                    last_tick = now

                    if active_seconds >= ACTIVE_DURATION:
                        done = True

                # ── Read one line from Arduino ────────────────────
                try:
                    raw = ser.readline()
                except serial.SerialException as e:
                    print(f"\n  [Serial error: {e}]")
                    break

                if not raw:
                    continue

                try:
                    line = raw.decode("utf-8").strip()
                except UnicodeDecodeError:
                    continue

                if not line:
                    continue

                # Status / comment lines from Arduino
                if line.startswith("#"):
                    print(f"  Arduino: {line[1:].strip()}")
                    continue

                # Skip CSV header line if Arduino sends it
                if "timestamp_ms" in line:
                    continue

                # ── Write row only when actively recording ────────
                if not paused and not done:
                    parts = line.split(",")
                    if len(parts) == 3:
                        csv_file.write(line + "\n")
                        rows_written += 1

                        # Flush to disk every 25 rows (~5 seconds)
                        if rows_written % 25 == 0:
                            csv_file.flush()

                        # Progress update every 150 rows (30 seconds)
                        if rows_written % 150 == 0:
                            remaining = ACTIVE_DURATION - active_seconds
                            print(
                                f"  {format_time(active_seconds)} elapsed  |  "
                                f"{format_time(remaining)} remaining  |  "
                                f"{rows_written} rows"
                            )

    except KeyboardInterrupt:
        print()
        print("  Aborted with Ctrl+C.")
        print(f"  {rows_written} rows saved before abort.")

    finally:
        input_thread_stop.set()
        ser.close()

    # ── Summary ───────────────────────────────────────────────────
    print()
    print("=" * 57)
    print("   SESSION COMPLETE")
    print("=" * 57)
    print(f"   Condition   : {time_of_day} / {location} / {weather}")
    print(f"   Active time : {format_time(active_seconds)}")
    print(f"   Rows saved  : {rows_written}")
    print(f"   File        : {csv_filename}")
    print(f"   Location    : {script_dir}")
    print("=" * 57)
    print()

    if rows_written == 0:
        print("  WARNING: No rows were written.")
        print("  Check that the Arduino is sending data and the")
        print("  CSV format matches: timestamp_ms,LightMean,TempMean")

    elif rows_written < 500:
        print(f"  NOTE: Only {rows_written} rows collected.")
        print("  A full 4-minute session produces around 1,200 rows.")
        print("  Consider re-running this session.")

    else:
        print(f"  Good session. Expected ~1,200 rows, got {rows_written}.")
        print("  Next step: upload this CSV to Edge Impulse.")

    print()
    input("  Press ENTER to close.")


if __name__ == "__main__":
    main()
