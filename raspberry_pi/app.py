from flask import Flask, jsonify, render_template, request

app = Flask(__name__)

state = {
    "opened": False,
    "monitoring": False,
    "interval_ms": 1000,
    "threshold_cm": 20.0,
    "last_message": "Aplikácia pripravená.",
}


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/open", methods=["POST"])
def api_open():
    state["opened"] = True
    state["monitoring"] = False
    state["last_message"] = "Systém bol inicializovaný."
    return jsonify(ok=True, message=state["last_message"], state=state)


@app.route("/api/settings", methods=["POST"])
def api_settings():
    payload = request.get_json(silent=True) or {}
    interval_ms = int(payload.get("interval_ms", state["interval_ms"]))
    threshold_cm = float(payload.get("threshold_cm", state["threshold_cm"]))

    interval_ms = max(200, min(10000, interval_ms))
    threshold_cm = max(2.0, min(400.0, threshold_cm))

    state["interval_ms"] = interval_ms
    state["threshold_cm"] = threshold_cm
    state["last_message"] = "Parametre boli nastavené."

    return jsonify(ok=True, interval_ms=interval_ms, threshold_cm=threshold_cm)


@app.route("/api/start", methods=["POST"])
def api_start():
    if not state["opened"]:
        return jsonify(ok=False, message="Najprv klikni Open."), 400

    state["monitoring"] = True
    state["last_message"] = "Monitorovanie spustené."
    return jsonify(ok=True, message=state["last_message"], state=state)


@app.route("/api/stop", methods=["POST"])
def api_stop():
    state["monitoring"] = False
    state["last_message"] = "Monitorovanie zastavené."
    return jsonify(ok=True, message=state["last_message"], state=state)


@app.route("/api/close", methods=["POST"])
def api_close():
    state["opened"] = False
    state["monitoring"] = False
    state["last_message"] = "Systém ukončený."
    return jsonify(ok=True, message=state["last_message"], state=state)


@app.route("/api/state")
def api_state():
    return jsonify(state)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
