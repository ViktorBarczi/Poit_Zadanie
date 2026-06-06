import os
import threading
import time
from pathlib import Path

import serial
from flask import Flask, jsonify, render_template, request

APP_DIR = Path(__file__).resolve().parent
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
    "last_data": None,
    "last_message": "Aplikácia pripravená.",
    "errors": [],
}

recent_data = []
MAX_RECENT = 100


def serial_write(command):
    global ser
    with serial_lock:
        if ser is None or not ser.is_open:
            raise RuntimeError("Serial port nie je otvorený. Najprv klikni Open.")
        ser.write((command + "\n").encode("utf-8"))
        ser.flush()


def parse_data_line(line):
    raw = line.strip()
    if not raw.startswith("DATA:"):
        return None

    parts = raw[5:].split(",")
    if len(parts) < 4:
        return None

    with state_lock:
        interval_ms = state["interval_ms"]

    return {
        "distance_cm": float(parts[0]),
        "threshold_cm": float(parts[1]),
        "alert": int(parts[2]),
        "hw_state": parts[3],
        "interval_ms": interval_ms,
    }


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

            if raw.startswith("DATA:"):
                item = parse_data_line(raw)
                if item:
                    with state_lock:
                        state["last_data"] = item
                        recent_data.append(item)
                        if len(recent_data) > MAX_RECENT:
                            del recent_data[0:len(recent_data) - MAX_RECENT]
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

    return jsonify(ok=True, interval_ms=interval_ms, threshold_cm=threshold_cm)


@app.route("/api/start", methods=["POST"])
def api_start():
    try:
        serial_write(f"I{state['interval_ms']}")
        serial_write(f"D{state['threshold_cm']}")
        serial_write("S")
        with state_lock:
            state["monitoring"] = True
        return jsonify(ok=True, message="Monitorovanie spustené.")
    except Exception as exc:
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/stop", methods=["POST"])
def api_stop():
    try:
        serial_write("T")
        with state_lock:
            state["monitoring"] = False
        return jsonify(ok=True, message="Monitorovanie zastavené.")
    except Exception as exc:
        return jsonify(ok=False, message=str(exc)), 500


@app.route("/api/close", methods=["POST"])
def api_close():
    global ser
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


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
