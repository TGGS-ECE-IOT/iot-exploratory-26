from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route("/predict", methods=["POST"])
def predict():
    data = request.json

    motion = data["motion"]
    light = data["light"]
    lux = data["lux"]
    sound = data["sound"]

    # simple logic (pretend it's cloud AI)
    if motion == 1 and light == 0 and sound > 65:
        return jsonify({"status": "alert"})
    else:
        return jsonify({"status": "normal"})

app.run(host="0.0.0.0", port=5000)