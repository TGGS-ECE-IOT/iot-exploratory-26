from openai import OpenAI

client = OpenAI(api_key="YOUR_API_KEY")

def ask_llm(motion, light, lux, sound):
    prompt = f"""
You are an IoT monitoring system.

Sensor data:
- Motion: {motion}
- Light: {light}
- Lux: {lux}
- Sound: {sound}

Is this situation NORMAL or ANOMALY?
Answer only one word: NORMAL or ALERT.
"""

    response = client.chat.completions.create(
        model="gpt-4o-mini",
        messages=[{"role": "user", "content": prompt}],
        temperature=0
    )

    return response.choices[0].message.content.strip()

# Example
result = ask_llm(1, 0, 120, 70)
print("LLM Result:", result)