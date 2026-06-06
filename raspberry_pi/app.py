import csv
import os
import sqlite3
import threading
import time
from datetime import datetime
from pathlib import Path

import serial
from flask import Flask, jsonify, render_template, request

APP_DIR = Path(__file__).resolve().parent
DATA_DIR = APP_DIR / "data"
DATA_DIR.mkdir(exist_ok=True)

DB_PATH = DATA_DIR / "iot_archive.sqlite3"
CSV_PATH = DATA_DIR / "iot_archive.csv"

DEFAULT_SERIAL_PORT = os.environ.get("SERIAL_PORT", "/dev/ttyACM0")
DEFAULT_BAUDRATE = int(os.environ.get("SERIAL_BAUDRATE", "115200"))

app = Flask(__name__)

state_lock = threading.Lock()
serial_lock = threading.Lock()

ser = None
reader_thread = None
reader_running = False

state = {
    "opened": False,
    "monitoring": False,
    "serial_port": DEFAULT_SERIAL_PORT,
    "baudrate": DEFAULT_BAUDRATE,
    "interval_ms": 1000,
    "threshold_cm": 20.0,
    "general_color": "GREEN",
    "alarm_color": "RED",
    "last_data": None,
    "last_message": "Aplikácia pripravená.",
    "errors": [],
}

recent_data = []
MAX_RECENT = 100


def db_connect():
    return sqlite3.connect(DB_PATH, check_same_thread=False)


def init_storage():
    with db_connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS measurements (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                created_at TEXT NOT NULL,
                esp_millis INTEGER,
                distance_cm REAL,
                threshold_cm REAL,
                alert INTEGER,
                led TEXT,
                buzzer INTEGER,
                interval_ms INTEGER
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                created_at TEXT NOT NULL,
                event_type TEXT NOT NULL,
                message TEXT NOT NULL,
                interval_ms INTEGER,
                threshold_cm REAL
            )
            """
        )

    if not CSV_PATH.exists():
        with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                "created_at",
                "esp_millis",
                "distance_cm",
                "threshold_cm",
                "alert",
                "led",
                "buzzer",
                "interval_ms",
            ])


def log_event(event_type, message):
    created_at = datetime.now().isoformat(timespec="seconds")
    with state_lock:
        interval_ms = state["interval_ms"]
        threshold_cm = state["threshold_cm"]
        state["last_message"] = message

    with db_connect() as conn:
        conn.execute(
            "INSERT INTO events(created_at, event_type, message, interval_ms, threshold_cm) VALUES (?, ?, ?, ?, ?)",
            (created_at, event_type, message, interval_ms, threshold_cm),
        )


def save_measurement(item):
    with db_connect() as conn:
        conn.execute(
            """
            INSERT INTO measurements(created_at, esp_millis, distance_cm, threshold_cm, alert, led, buzzer, interval_ms)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                item["created_at"],
                item["esp_millis"],
                item["distance_cm"],
                item["threshold_cm"],
                item["alert"],
                item["led"],
                item["buzzer"],
                item["interval_ms"],
            ),
        )

    with CSV_PATH.open("a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            item["created_at"],
            item["esp_millis"],
            item["distance_cm"],
            item["threshold_cm"],
            item["alert"],
            item["led"],
            item["buzzer"],
            item["interval_ms"],
        ])


def parse_data_line(line):
    """
    Supported ESP32 format from updated Arduino code:
      DATA:<distance_cm>,<threshold_cm>,<alert>,<state>

    Older comma format is also accepted:
      DATA,<millis>,<distance_cm>,<threshold_cm>,<alert>,<led>,<buzzer>
    """
    raw = line.strip()

    with state_lock:
        interval_ms = state["interval_ms"]

    if raw.startswith("DATA:"):
        parts = raw[5:].split(",")
        if len(parts) < 4:
            return None

        distance_cm = float(parts[0])
        threshold_cm = float(parts[1])
        alert = int(parts[2])
        hw_state = parts[3]

        return {
            "created_at": datetime.now().isoformat(timespec="seconds"),
            "esp_millis": None,
            "distance_cm": distance_cm,
            "threshold_cm": threshold_cm,
            "alert": alert,
            "led": state.get("alarm_color", "RED") if alert else state.get("general_color", "GREEN"),
            "buzzer": 1 if alert else 0,
            "interval_ms": interval_ms,
            "hw_state": hw_state,
        }

    parts = raw.split(",")
    if len(parts) != 7 or parts[0] != "DATA":
        return None

    return {
        "created_at": datetime.now().isoformat(timespec="seconds"),
        "esp_millis": int(parts[1]),
        "distance_cm": float(parts[2]),
        "threshold_cm": float(parts[3]),
        "alert": int(parts[4]),
        "led": parts[5],
        "buzzer": int(parts[6]),
        "interval_ms": interval_ms,
        "hw_state": "UNKNOWN",
    }


def serial_write(command):
    global ser
    with serial_lock:
        if ser is None or not ser.is_open:
            raise RuntimeError("Serial port nie je otvorený. Najprv klikni Open.")
        ser.write((command + "\n").encode("utf-8"))
        ser.flush()


def reader_loop():
    global reader_running, ser
    while reader_running:
        try:
            with serial_lock:
                current_ser = ser
            if current_ser is None or not current_ser.is_open:
                time.sleep(0.2)
                continue

            raw = current_ser.readline().decode("utf-8", errors="replace").strip()
            if not raw:
                continue

            if raw.startswith("DATA:") or raw.startswith("DATA,"):
                item = parse_data_line(raw)
                if item:
                    save_measurement(item)
                    with state_lock:
                        state["last_data"] = item
                        recent_data.append(item)
                        if len(recent_data) > MAX_RECENT:
                            del recent_data[0 : len(recent_data) - MAX_RECENT]
            elif raw.startswith("ACK,"):
                with state_lock:
                    state["last_message"] = raw
            elif raw.startswith("ERR,"):
                with state_lock:
                    state["last_message"] = raw
                    state["errors"].append(raw)
                    state["errors"] = state["errors"][-10:]
            else:
                with state_lock:
                    state["last_message"] = raw
        except Exception as exc:
            with state_lock:
                state["last_message"] = f"Chyba čítania Serial: {exc}"
            time.sleep(1)


def start_reader_thread():
    global reader_thread, reader_running
    if reader_thread and reader_thread.is_alive():
        return
    reader_running = True
    reader_thread = threading.Thread(target=reader_loop, daemon=True)
    reader_thread.start()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/open", methods=["POST"])
def api_open():
    global ser
    payload = request.get_json(silent=True) or {}
    port = payload.get("port") or state["serial_port"]
    baudrate = int(payload.get("baudrate") or state["baudrate"])

    try:
        with serial_lock:
            if ser is not None and ser.is_open:
                ser.close()
            ser = serial.Serial(port, baudrate, timeout=1)
            time.sleep(2)

        with state_lock:
            state["opened"] = True
            state["monitoring"] = False
            state["serial_port"] = port
            state["baudrate"] = baudrate

        start_reader_thread()
        serial_write("O")
        log_event("OPEN", f"Spojenie otvorené na porte {port}.")
        return jsonify(ok=True, message="Systém inicializovaný.")
    except Exception as exc:
        with state_lock:
            state["opened"] = False
            state["last_message"] = str(exc)
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/settings", methods=["POST"])
def api_settings():
    payload = request.get_json(silent=True) or {}
    interval_ms = int(payload.get("interval_ms", state["interval_ms"]))
    threshold_cm = float(payload.get("threshold_cm", state["threshold_cm"]))

    interval_ms = max(200, min(10000, interval_ms))
    threshold_cm = max(2.0, min(400.0, threshold_cm))

    with state_lock:
        state["interval_ms"] = interval_ms
        state["threshold_cm"] = threshold_cm

    if state["opened"]:
        serial_write(f"I{interval_ms}")
        serial_write(f"D{threshold_cm}")

    log_event("SETTINGS", f"Nastavené parametre: interval {interval_ms} ms, prah {threshold_cm:.1f} cm.")
    return jsonify(ok=True, interval_ms=interval_ms, threshold_cm=threshold_cm)


@app.route("/api/start", methods=["POST"])
def api_start():
    with state_lock:
        interval_ms = state["interval_ms"]
        threshold_cm = state["threshold_cm"]

    try:
        serial_write(f"I{interval_ms}")
        serial_write(f"D{threshold_cm}")
        serial_write("S")
        with state_lock:
            state["monitoring"] = True
        log_event("START", "Monitorovanie/regulácia spustená.")
        return jsonify(ok=True, message="Monitorovanie spustené.")
    except Exception as exc:
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/stop", methods=["POST"])
def api_stop():
    try:
        serial_write("T")
        with state_lock:
            state["monitoring"] = False
        log_event("STOP", "Monitorovanie/regulácia zastavená.")
        return jsonify(ok=True, message="Monitorovanie zastavené.")
    except Exception as exc:
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/close", methods=["POST"])
def api_close():
    global ser, reader_running
    try:
        if state["opened"]:
            serial_write("C")
        time.sleep(0.2)
        with serial_lock:
            if ser is not None and ser.is_open:
                ser.close()
        with state_lock:
            state["opened"] = False
            state["monitoring"] = False
        log_event("CLOSE", "Spojenie ukončené a akčné členy deaktivované.")
        return jsonify(ok=True, message="Systém ukončený.")
    except Exception as exc:
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/state")
def api_state():
    with state_lock:
        return jsonify(state)


@app.route("/api/latest")
def api_latest():
    with state_lock:
        return jsonify(data=state["last_data"], state=state)


@app.route("/api/recent")
def api_recent():
    with state_lock:
        return jsonify(data=list(recent_data))


@app.route("/api/archive/db")
def api_archive_db():
    limit = int(request.args.get("limit", 200))
    limit = max(1, min(1000, limit))
    with db_connect() as conn:
        conn.row_factory = sqlite3.Row
        rows = conn.execute(
            "SELECT * FROM measurements ORDER BY id DESC LIMIT ?",
            (limit,),
        ).fetchall()
    return jsonify(data=[dict(row) for row in rows][::-1])


@app.route("/api/archive/events")
def api_archive_events():
    with db_connect() as conn:
        conn.row_factory = sqlite3.Row
        rows = conn.execute("SELECT * FROM events ORDER BY id DESC LIMIT 100").fetchall()
    return jsonify(data=[dict(row) for row in rows])


COLOR_TO_CODE = {
    "BLUE": 1,
    "GREEN": 2,
    "RED": 3,
    "PURPLE": 4,
}


@app.route("/api/settings/colors", methods=["POST"])
def api_settings_colors():
    payload = request.get_json(silent=True) or {}
    general_color = str(payload.get("general_color", state.get("general_color", "GREEN"))).upper()
    alarm_color = str(payload.get("alarm_color", state.get("alarm_color", "RED"))).upper()

    if general_color not in COLOR_TO_CODE:
        general_color = "GREEN"
    if alarm_color not in COLOR_TO_CODE:
        alarm_color = "RED"

    with state_lock:
        state["general_color"] = general_color
        state["alarm_color"] = alarm_color

    if state["opened"]:
        serial_write(f"G{COLOR_TO_CODE[general_color]}")
        serial_write(f"A{COLOR_TO_CODE[alarm_color]}")

    log_event("COLORS", f"Nastavené farby: všeobecná {general_color}, alarm {alarm_color}.")
    return jsonify(ok=True, general_color=general_color, alarm_color=alarm_color, message="Farby boli odoslané do ESP32.")


@app.route("/api/control/led", methods=["POST"])
def api_control_led():
    payload = request.get_json(silent=True) or {}
    color = str(payload.get("color", "OFF")).upper()
    if color not in {"OFF", "RED", "GREEN", "BLUE"}:
        color = "OFF"
    serial_write(f"G{COLOR_TO_CODE.get(color, 2)}")
    log_event("ACTION", f"Manuálne nastavenie LED: {color}.")
    return jsonify(ok=True, color=color)


@app.route("/api/control/buzzer", methods=["POST"])
def api_control_buzzer():
    payload = request.get_json(silent=True) or {}
    enabled = bool(payload.get("enabled", False))
    serial_write("BUZZER,ON" if enabled else "BUZZER,OFF")
    log_event("ACTION", f"Manuálne nastavenie bzučiaka: {'ON' if enabled else 'OFF'}.")
    return jsonify(ok=True, enabled=enabled)


if __name__ == "__main__":
    init_storage()
    app.run(host="0.0.0.0", port=5000, debug=False)
