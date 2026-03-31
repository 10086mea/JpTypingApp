import sys
import os
import json
import urllib.request
import urllib.error
import logging
import struct
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

# 设定日志配置
log_level = logging.DEBUG if "--debug" in sys.argv else logging.INFO
logging.basicConfig(
    level=log_level, 
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)

# 尝试导入 google-genai 用于 TTS
try:
    from google import genai
    from google.genai import types
    has_genai = True
except ImportError as e:
    logging.warning(f"Failed to import google-genai: {e}. TTS feature will be disabled.")
    has_genai = False

config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
try:
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)
    API_KEY = config.get("API_KEY", "")
except Exception as e:
    logging.warning(f"Failed to load config.json: {e}")
    API_KEY = ""

URL = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={API_KEY}"

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

def analyze_text(text, mode="completion"):
    if mode == "completion":
        prompt = f"""
You are an advanced Japanese language autocomplete assistant.
The user is typing: '{text}'
Predict the next few characters or words (maximum 10 characters) the user is most likely to type next.
Respond ONLY with the predicted continuation characters. No explanation, no markdown, no JSON formatting.
If the input is empty or invalid, respond with an empty string.
"""
        data = {
            "contents": [{"parts": [{"text": prompt}]}],
            "generationConfig": {
                "temperature": 0.1,
                "maxOutputTokens": 20
            }
        }
    else:
        prompt = f"""
You are an advanced Japanese language assistant.
The user is typing Japanese in a typing practice application.
Their current input is: '{text}'

Analyze the currently input text. Find any grammatical errors, unnatural phrasing, or typos, and provide annotations. If there are no errors, return an empty list for grammar_errors.

Respond ONLY with a valid JSON object in the exact following structure. Do not include markdown formatting like ```json.
{{
  "completion": "predicted next characters/words",
  "grammar_errors": [
    {{
      "wrong_text": "text that is wrong",
      "correction": "suggested correction",
      "explanation": "explanation of the error in Chinese"
    }}
  ]
}}
"""
        data = {
            "contents": [{"parts": [{"text": prompt}]}],
            "generationConfig": {
                "temperature": 0.2,
                "responseMimeType": "application/json"
            }
        }
    
    req_body = json.dumps(data).encode('utf-8')
    logging.debug(f"Gemini Request Payload: {req_body.decode('utf-8')}")
    req = urllib.request.Request(URL, data=req_body, headers={'Content-Type': 'application/json'})
    
    try:
        response = urllib.request.urlopen(req)
        response_body = response.read().decode('utf-8')
        logging.debug(f"Gemini Response Body: {response_body}")
        
        result_json = json.loads(response_body)
        content_text = result_json.get('candidates', [{}])[0].get('content', {}).get('parts', [{}])[0].get('text', '').strip()
        
        if mode == "completion":
            return {"completion": content_text, "grammar_errors": []}
        else:
            try:
                return json.loads(content_text)
            except json.JSONDecodeError:
                if content_text.startswith("```json"):
                    content_text = content_text[7:]
                if content_text.endswith("```"):
                    content_text = content_text[:-3]
                return json.loads(content_text.strip())
        
    except Exception as e:
        logging.error(f"Gemini API Error: {e}")
        return {"error": str(e), "completion": "", "grammar_errors": []}

def chat_and_tts(text, system_prompt):
    if not has_genai:
        return {"reply": "缺少 google-genai 库", "audio_file": ""}
    
    if not text.strip():
        return {"reply": "", "audio_file": ""}
        
    client = genai.Client(api_key=API_KEY)
    
    # 1. Generate text response using gemini-2.5-flash
    reply_text = ""
    try:
        response = client.models.generate_content(
            model='gemini-2.5-flash',
            contents=text,
            config=types.GenerateContentConfig(
                system_instruction=system_prompt,
                temperature=0.8,
            ),
        )
        reply_text = response.text.strip()
    except Exception as e:
        logging.error(f"Chat generation failed: {e}")
        return {"reply": f"对话生成失败: {e}", "audio_file": ""}
        
    # 2. Generate TTS for the replied text
    cache_dir = os.path.join(os.getcwd(), "audio_cache")
    os.makedirs(cache_dir, exist_ok=True)
    filename = f"reply_{uuid.uuid4().hex[:8]}.wav"
    temp_wav_path = os.path.join(cache_dir, filename)
    try:
        # User defined snippet config
        contents = [
            types.Content(
                role="user",
                parts=[
                    types.Part.from_text(text="Read aloud in a warm and friendly tone: " + reply_text),
                ],
            ),
        ]
        generate_content_config = types.GenerateContentConfig(
            temperature=1,
            response_modalities=["audio"],
            speech_config=types.SpeechConfig(
                voice_config=types.VoiceConfig(
                    prebuilt_voice_config=types.PrebuiltVoiceConfig(
                        voice_name="Zephyr"
                    )
                )
            ),
        )
        
        tts_response = client.models.generate_content(
            model="gemini-2.5-pro-preview-tts",
            contents=contents,
            config=generate_content_config,
        )
        
        if tts_response.parts and tts_response.parts[0].inline_data:
            inline_data = tts_response.parts[0].inline_data
            mime_type = inline_data.mime_type or "audio/L16;rate=24000"
            if "audio/L" in mime_type:
                wav_data = convert_to_wav(inline_data.data, mime_type)
            else:
                wav_data = inline_data.data
                
            with open(temp_wav_path, "wb") as f:
                f.write(wav_data)
        else:
            logging.error("No audio data in TTS response.")
    except Exception as e:
        logging.error(f"TTS generation failed: {e}")
        
    # Always return the text, even if TTS failed, return the potentially valid audio path or empty string
    return {"reply": reply_text, "audio_file": temp_wav_path if os.path.exists(temp_wav_path) else ""}

class RequestHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/analyze':
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            logging.debug(f"HTTP POST /analyze received {content_length} bytes")
            
            try:
                request_json = json.loads(post_data.decode('utf-8'))
                text = request_json.get('text', '')
                mode = request_json.get('mode', 'completion')
                logging.info(f"📝 收到打字请求文本: {repr(text)} [模式: {mode}]")
                
                result = analyze_text(text, mode)
                logging.info(f"✨ 返回分析结果: 补全='{result.get('completion','')}' | 发现语法错误={len(result.get('grammar_errors',[]))}处")
                logging.debug(f"Full JSON Response: {json.dumps(result, ensure_ascii=False)}")
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.end_headers()
                self.wfile.write(json.dumps(result, ensure_ascii=False).encode('utf-8'))
            except ConnectionAbortedError:
                logging.warning("客户端已先行断开连接 (可能因为超时)，已终止发送。")
            except Exception as e:
                logging.error(f"处理请求时发生内部错误: {e}", exc_info=True)
                self._send_error(str(e))
                
        elif self.path == '/chat':
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            
            try:
                request_json = json.loads(post_data.decode('utf-8'))
                text = request_json.get('text', '')
                system_prompt = request_json.get('system_prompt', '你是一个AI助手')
                logging.info(f"💬 收到对话请求: {repr(text)}")
                
                result = chat_and_tts(text, system_prompt)
                logging.info(f"✨ 对话返回: {result.get('reply', '')} | 语音文件: {result.get('audio_file', '')}")
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.end_headers()
                self.wfile.write(json.dumps(result, ensure_ascii=False).encode('utf-8'))
            except Exception as e:
                logging.error(f"对话请求出错: {e}", exc_info=True)
                self._send_error(str(e))
        else:
            self.send_response(404)
            self.end_headers()
            
    def _send_error(self, error_msg):
        try:
            self.send_response(400)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.end_headers()
            self.wfile.write(json.dumps({"error": error_msg}).encode('utf-8'))
        except: pass

    def log_message(self, format, *args):
        logging.info("🌐 " + format % args)

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    def handle_error(self, request, client_address):
        import sys
        exctype, value = sys.exc_info()[:2]
        if exctype is not None and (issubclass(exctype, ConnectionAbortedError) or issubclass(exctype, ConnectionResetError)):
            return
        HTTPServer.handle_error(self, request, client_address)

def run_server(port=5000):
    server_address = ('127.0.0.1', port)
    httpd = ThreadedHTTPServer(server_address, RequestHandler)
    logging.info(f"🚀 Starting Gemini Local HTTP Service on http://127.0.0.1:{port}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logging.info("\n👋 Shutting down server...")
        httpd.server_close()

if __name__ == "__main__":
    run_server()
