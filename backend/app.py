import os
import subprocess
from flask import Flask, jsonify, render_template, request

app = Flask(
    __name__,
    template_folder="../frontend/templates",
    static_folder="../frontend/static",
)

ENGINE_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "dfa_engine.exe")
)
SCHEMA_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "config", "schema.json")
)


@app.route("/")
def index():
  return render_template("index.html")


@app.route("/api/evaluate", methods=["POST"])
def evaluate_transaction():
  data = request.get_json()
  if not data or "txn_id" not in data or "event" not in data:
    return (
        jsonify({"error": "Missing 'txn_id' or 'event' in JSON payload"}),
        400,
    )

  txn_id = data["txn_id"]
  event_name = data["event"]

  if not os.path.exists(ENGINE_PATH):
    return (
        jsonify({
            "error": "C++ engine binary not found. Please run 'make' first."
        }),
        500,
    )

  try:
    result = subprocess.run(
        [ENGINE_PATH, SCHEMA_PATH, txn_id, event_name],
        capture_output=True,
        text=True,
        check=True,
    )
    next_state_id = result.stdout.strip()
    return jsonify({
        "status": "success",
        "txn_id": txn_id,
        "event": event_name,
        "next_state_id": next_state_id,
    })
  except subprocess.CalledProcessError as e:
    return (
        jsonify({
            "error": "C++ engine execution failed",
            "details": e.stderr.strip(),
        }),
        500,
    )


@app.route("/api/metrics", methods=["GET"])
def get_metrics():
  try:
    result = subprocess.run(
        [ENGINE_PATH, SCHEMA_PATH, "__METRICS__", "DUMMY"],
        capture_output=True,
        text=True,
        check=True,
    )
    return jsonify({"status": "success", "metrics_raw": result.stdout.strip()})
  except Exception as e:
    return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
  app.run(debug=True, port=5000)