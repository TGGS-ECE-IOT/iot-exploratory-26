import time
import random
import requests
import numpy as np
from sklearn.ensemble import IsolationForest

# =========================
# CONFIG
# =========================
API_URL = "http://localhost:5000/predict"  # change to any API you want to use
USE_API = False # change to true if you are using API call

# =========================
# SENSOR SIMULATION (replace with real sensors)
# =========================
def read_sensors():
    """
    Replace these with real sensor readings
    """
    motion = random.choice([0, 1])          # 0 = no motion, 1 = motion
    light = random.choice([0, 1])           # 0 = dark, 1 = bright
    lux = random.uniform(100, 400)          # light intensity
    sound = random.uniform(30, 70)          # sound level (dB)

    return [motion, light, lux, sound]

# =========================
# TRAIN MODEL (normal behavior)
# =========================
def generate_training_data(n=100):
    """
    Generate 'normal' data for training
    """
    data = []
    for _ in range(n):
        motion = random.choice([0, 1])
        light = random.choice([0, 1])
        lux = random.uniform(150, 300)
        sound = random.uniform(40, 60)

        data.append([motion, light, lux, sound])
    return np.array(data)

print("Training Isolation Forest model...")

X_train = generate_training_data(200)

model = IsolationForest(contamination=0.1, random_state=42)
model.fit(X_train)

print("Model ready.\n")

# =========================
# API CALL
# =========================
def get_api_result(data):
    try:
        payload = {
            "motion": int(data[0]),
            "light": int(data[1]),
            "lux": float(data[2]),
            "sound": float(data[3])
        }

        response = requests.post(API_URL, json=payload, timeout=2)
        result = response.json()

        return result.get("status", "unknown")

    except Exception as e:
        print("API error:", e)
        return "unknown"

# =========================
# MAIN LOOP
# =========================
print("Starting monitoring...\n")

while True:
    data = read_sensors()
    motion, light, lux, sound = data

    # Edge prediction
    pred = model.predict([data])[0]  # 1 = normal, -1 = anomaly
    edge_result = "alert" if pred == -1 else "normal"

    # API prediction (second opinion)
    if USE_API:
        api_result = get_api_result(data)
    else:
        api_result = "disabled"

    # Combine decision
    if edge_result == "alert" or api_result == "alert":
        final = "ALERT ⚠️"
    else:
        final = "NORMAL ✅"

    # Print results
    print(f"""
Motion: {motion} | Light: {light} | Lux: {lux:.1f} | Sound: {sound:.1f}
Edge AI: {edge_result}
API AI:  {api_result}
FINAL:   {final}
-------------------------
""")

    time.sleep(2)