# Data Analytics & AI for IoT

---

## Part 1 — Introduction

### What is Data Analytics, ML, and AI?

- **Data Analytics**
  - Understand what happened, e.g., process of examining, clening, transforming, and modeling raw data to uncover insights which can the support decision making process
  - Example: average temperature, trends

- **Artificial Intelligence (AI)**
  - Broad concept of machines simulating human intelligence to solve problems

- **Machine Learning (ML)**
  - A subset of AI, focus on training systems to learn patterns from data and improve accuracy *without explicit programming*
  - Predict or classify

In a sense, AI is the goal, and ML is a commonm technique used to achieve it.

> Data → Insight → Prediction → Action

---

### Types of Analytics

- **Descriptive**
  - Tell what happended

- **Diagnostic**
  - Explain why something happended

- **Predictive Analytics**
  - Predicts what is likely to happen in the future

- **Prescriptive**
  - Suggest action to achive some objective


---

### Common AI Models

- Rule-based systems (the traditional)
- Machine Learning models
- Deep Learning
- Generative AI (e.g., LLMs)

---

### Common Use Cases

- Recommendation systems (Netflix, YouTube)
- Spam detection
- Speech recognition
- Image recognition

#### Example: Simple text generation

```python
from google.colab import ai

response = ai.generate_text("What is the capital of France?")
print(response)
```

---

## Part 2 — AI in Resource-Constrained Devices

### Challenges

- Limited CPU / memory
- Power consumption
- Latency requirements

---

### Edge AI vs Cloud AI

**Edge (ESP32 / Raspberry Pi)**  
- (+) Fast response  
- (+) Works offline  
- (-) Limited compute power  

**Cloud (API / server)**  
- (+) Powerful models  
- (+) Easy to use  
- (-) Requires internet  
- (-) Latency  

---

### Simple Task: Anomaly Detection

> Detect unusual behavior instead of fixed rules

Example:
> Motion + low light → suspicious

---

#### From Rule-Based to AI

- Rule:
  - `if motion AND dark → alert`

- Data-driven:
  - learn what is normal (find the pattern)
  - detect deviation

Example:

```python
# Simple rule-based anomaly detection

motion = 1      # 1 = motion detected
light = 0       # 0 = dark, 1 = bright

if motion == 1 and light == 0:
    print("ALERT: Motion in dark")
else:
    print("NORMAL")
```

or with some statistical analysis

```python
import numpy as np

# Example historical data (light levels)
history = [30, 32, 31, 29, 30, 31, 30, 32, 29, 30]

# New reading
new_value = 45

mean = np.mean(history)
std = np.std(history)

z_score = abs((new_value - mean) / std)

if z_score > 2:
    print("ALERT: Anomaly detected")
else:
    print("NORMAL")
```

or be smarter with time (using moving average)

```
import pandas as pd

data = [30, 31, 29, 30, 32, 31, 30, 45]  # last value is anomaly

df = pd.DataFrame(data, columns=["light"])

df["moving_avg"] = df["light"].rolling(window=3).mean()
df["diff"] = abs(df["light"] - df["moving_avg"])

threshold = 5

df["anomaly"] = df["diff"] > threshold

print(df)
```

---

#### Simple ML Model

- Predictive model (e.g., classification, anomaly detection)
- Input: `motion`, `light`
- Output: `normal` / `alert`

Example: the supervised classification

```python
from sklearn.tree import DecisionTreeClassifier

# Features: [motion, light]
X = [
    [0, 1],
    [1, 1],
    [0, 0],
    [1, 0]
]

# Labels: 0 = normal, 1 = anomaly
y = [0, 0, 0, 1]

model = DecisionTreeClassifier()
model.fit(X, y)

# New data
new_data = [[1, 0]]  # motion + dark

prediction = model.predict(new_data)

if prediction[0] == 1:
    print("ALERT: Anomaly detected")
else:
    print("NORMAL")
```
or what if we do so without label (the unsupervised approach)

```python
from sklearn.ensemble import IsolationForest
import numpy as np

# Training data (normal)
X_train = np.array([[30], [31], [29], [30], [32], [31]])

model = IsolationForest(contamination=0.1)
model.fit(X_train)

# New data
X_test = np.array([[30], [45]])

pred = model.predict(X_test)

for val, p in zip(X_test, pred):
    if p == -1:
        print(f"{val[0]} → ANOMALY")
    else:
        print(f"{val[0]} → NORMAL")
```

or you might curious with the popular **Deep Learning** (demo only)

```python
import numpy as np
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense

# Fake dataset
X = np.array([[30], [31], [29], [30], [32], [31]])
y = X  # autoencoder-style (reconstruct input)

model = Sequential([
    Dense(4, activation='relu', input_shape=(1,)),
    Dense(1)
])

model.compile(optimizer='adam', loss='mse')
model.fit(X, y, epochs=50, verbose=0)

# Test
test = np.array([[30], [45]])
pred = model.predict(test)

errors = np.abs(pred - test)

for val, err in zip(test, errors):
    if err > 5:
        print(f"{val[0]} → ANOMALY")
    else:
        print(f"{val[0]} → NORMAL")
```

Note: Model learns to reconstruct normal data --> large error = anomaly


---

#### Alternative: Offload to Server

- Send data via API
- Server processes with AI model
- Return result

Example template:

Send `motion` + `light` to a server
```
import requests

url = "https://example.com/predict"  # your API endpoint

data = {
    "motion": 1,
    "light": 0
}

response = requests.post(url, json=data)

print("Response:", response.json())
```

Assume API resturns `{ "status": "alert" }`

You can then use that to make a decision, e.g., 

```python
import requests

url = "https://example.com/predict"

data = {
    "motion": 1,
    "light": 0
}

try:
    response = requests.post(url, json=data)
    result = response.json()

    if result["status"] == "alert":
        print("ALERT ⚠️ from API")
    else:
        print("NORMAL")

except Exception as e:
    print("Error:", e)
```


---

## Part 3 — Building Your Model

### Python Ecosystem

- **Scikit-learn**
  - Simple ML models
  - Classification, regression

- **Basic workflow**
  - Load data
  - Train model
  - Predict

---

### Example ML Flow

1. Collect data  
2. Label data  
3. Train model  
4. Predict new data  

---

### Other AI Capabilities

- NLP (text processing, chatbot)
- Speech-to-text (transcription)
- Pretrained models

---

### Off-the-Shelf Models

Why Use Existing Models?

- No need to build everything from scratch  
- Pretrained on large datasets  
- Easy to integrate into your system  

> Focus on using AI, not building it from zero

Common Categories:

- **Image / Vision**
  - MobileNetV2
    - Lightweight, runs on small devices
  - ResNet
    - More accurate, but heavier
  - OpenCV (non-ML)
    - Face detection, Motion detection

  > They are mostly used for understanding images and video

- **Sensor Data**
  - Isolation Forest  
    - Detect anomalies without labels  
  - One-Class SVM  
    - Learn normal behavior  
  - Linear Regression  
    - Predict future values

  > Good for many IoT sensors data

- **Time Series & Prediction**
  - ARIMA
    - Forecast future values
  - Moving Average
    - Smooth data, detect pattern and trends

  > Useful for monitoring and prediction

- **NLP & Speech Models**  
  - BERT
    - Text classification
  - Whisper
    - Speech-to-text
  - GPT-2 (getting more heavier here)
    - Text generation
- **Edge / Embedded Models**
  - Tensorflow Lite
    - Run models on Raspberry Pi
  - TinyML
    - Very small models for microcontrollers

  > Deploy AI direct on edges

*Note: Camera not included in this workshop for your challenge. So many library here is a bit useless for now.*


---

## Part 4 — LLM and AGI

### What are LLMs?

- LLM = Large Language Model
- Trained on large amounts of text data
- Can understand and generate human-like language  

Examples:
- Chatbots  
- Code assistants  
- Question answering systems  

> LLMs predict the next word to generate meaningful text

---

### What can LLM do?

- Answer questions  
- Summarize text  
- Generate code  
- Translate languages  
- Assist with problem solving

Example Use in IoT

- Explain sensor data in natural language  
- Generate alerts:
  - “Temperature is unusually high”  
- Create simple reports automatically  

> Turning data into human-friendly insight

---

### Can they run on devices?

Short answer: **Yes, but with limitations**

**On Raspberry Pi:**
- Small models only  
- Slower performance  

**On Cloud (API):**
- Very powerful models  
- Fast and accurate  

---

### Local vs Cloud LLM

**Local (on device)**  
- (+) Works offline  
- (+) More private  
- (-) Limited capability  

**Cloud (API)**  
- (+) High performance  
- (+) Easy to use  
- (-) Requires internet (and tokens)


### Considerations

- Memory usage
- Performance
- Response time


---

### What is AGI?

- AGI = Artificial General Intelligence  
- A system that can perform any intellectual task like humans

Examples:
- Learning new tasks without retraining  
- General reasoning across domains  

#### Reality Check

- LLMs is **not AGI**  (and many believes, it will not be one)
- LLMs are powerful but specialized 
- Still researchers argues that it is one the crucial stepping stones to help us build AGI.

### Key Considerations

- Resource requirements (memory, CPU)  
- Latency (response time)  
- Cost (for API usage)  


---

## Part 5 — IoT + AI Architecture

### Typical Architecture

`Device → Network → Server → AI → Application → User`

---

### Design Choices

- Run AI on device (edge)
- Run AI on server (API)

Both are valid

### When to Use AI in IoT

Use when:
- You do not easily understand the data pattern
- You need human-friendly interaction
- You want to explain data
- You need flexible responses

Avoid when:
- Simple rules are enough  
- Real-time constraints are strict  
- System must be deterministic  

> Combine IoT + Data + AI + LLM for smarter applications

---

## Part 6 — Combining AI Approaches

### Compare Results

- Local model vs API result

---

### Hybrid System

- Use both edge + cloud

Example:
- Edge → quick decision
- Cloud → more accurate analysis

---

### Conflict Handling

- What if results differ?

Options:
- Prioritize one source
- Combine results
- Add rules

---

## Part 7 — When to Use AI?


| Approach  | Speed   | Power   | Internet |
| --------- | ------- | ------- | -------- |
| Local ML  | Fast  | Medium  | ❌        |
| Local LLM | Slow | Low     | ❌        |
| Cloud AI  | Fast  | High | ✅        |



### Use AI when:

- Patterns are complex
- Rules are hard to define
- Need prediction

---

### Avoid AI when:

- Simple rules work
- Data is limited
- System must be deterministic

---

### Final Thought

> AI is a tool, not always the solution

---

## Wrap-up

- IoT generates data  
- Analytics gives insight  
- AI enables smarter decisions  

> Build simple → then improve with AI