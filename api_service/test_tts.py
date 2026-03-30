import os
import json
import struct
import logging

try:
    from google import genai
    from google.genai import types
except ImportError as e:
    print(f"Error importing google-genai: {e}")

config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "api_service", "config.json")
try:
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)
    API_KEY = config.get("API_KEY", "")
except Exception as e:
    API_KEY = ""

def parse_audio_mime_type(mime_type: str) -> dict:
    parts = mime_type.split(";")
    bits_per_sample = 16
    rate = 24000
    for param in parts:
        param = param.strip()
        if param.lower().startswith("rate="):
            try:
                rate = int(param.split("=", 1)[1])
            except: pass
        elif param.startswith("audio/L"):
            try:
                bits_per_sample = int(param.split("L", 1)[1])
            except: pass
    return {"bits_per_sample": bits_per_sample, "rate": rate}

def convert_to_wav(audio_data: bytes, mime_type: str) -> bytes:
    parameters = parse_audio_mime_type(mime_type)
    bits_per_sample = parameters["bits_per_sample"]
    sample_rate = parameters["rate"]
    num_channels = 1
    data_size = len(audio_data)
    bytes_per_sample = bits_per_sample // 8
    block_align = num_channels * bytes_per_sample
    byte_rate = sample_rate * block_align
    chunk_size = 36 + data_size

    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF", chunk_size, b"WAVE", b"fmt ", 16, 1, num_channels,
        sample_rate, byte_rate, block_align, bits_per_sample, b"data", data_size
    )
    return header + audio_data

def test_tts():
    if not API_KEY:
        print("No API KEY found")
        return
        
    client = genai.Client(api_key=API_KEY)
    reply_text = "こんにちは！私は元気な女子高生AIだよ！"
    print(f"Generating TTS for: {reply_text}")

    model = "gemini-2.5-pro-preview-tts"
    contents = [
        types.Content(
            role="user",
            parts=[types.Part.from_text(text="Read aloud in a warm and friendly tone: " + reply_text)],
        ),
    ]
    config = types.GenerateContentConfig(
        temperature=1,
        response_modalities=["audio"],
        speech_config=types.SpeechConfig(
            voice_config=types.VoiceConfig(
                prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name="Zephyr")
            )
        ),
    )
    
    response = client.models.generate_content(
        model=model,
        contents=contents,
        config=config,
    )
    
    if response.parts and response.parts[0].inline_data:
        inline_data = response.parts[0].inline_data
        mime_type = inline_data.mime_type
        print(f"Generated audio with mime_type: {mime_type}, size: {len(inline_data.data)} bytes")
        
        if "audio/L" in mime_type:
            wav_data = convert_to_wav(inline_data.data, mime_type)
        else:
            wav_data = inline_data.data
            
        with open("test_tts.wav", "wb") as f:
            f.write(wav_data)
        print("Saved to test_tts.wav")
        import winsound
        winsound.PlaySound("test_tts.wav", winsound.SND_FILENAME)
    else:
        print("No audio data found in response")

if __name__ == "__main__":
    test_tts()
