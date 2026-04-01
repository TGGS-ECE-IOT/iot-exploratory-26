import google.generativeai as genai

# Set your API key
genai.configure(api_key="YOUR_API_KEY")

# Load model
model = genai.GenerativeModel("gemini-1.5-flash")

def ask_gemini(motion, light, lux, sound):
    prompt = f"""
You are an IoT monitoring system.

Sensor data:
- Motion: {motion}
- Light: {light}
- Lux: {lux}
- Sound: {sound}

Decide if this situation is NORMAL or ALERT.

Rules:
- Motion in dark may be suspicious
- Very high sound may be abnormal

Answer ONLY one word: NORMAL or ALERT.
"""

    response = model.generate_content(prompt)

    text = response.text.strip().upper()

    if "ALERT" in text:
        return "alert"
    else:
        return "normal"


# Test
result = ask_gemini(1, 0, 120, 75)
print("Gemini Result:", result)