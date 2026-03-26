import sys
import os
import json
import urllib.request
import urllib.error
import logging
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

# 设定日志配置
log_level = logging.DEBUG if "--debug" in sys.argv else logging.INFO
logging.basicConfig(
    level=log_level, 
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)

config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
try:
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)
    API_KEY = config.get("API_KEY", "")
except Exception as e:
    logging.warning(f"Failed to load config.json: {e}")
    API_KEY = ""

URL = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={API_KEY}"

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
                try:
                    self.send_response(400)
                    self.send_header('Content-Type', 'application/json; charset=utf-8')
                    self.end_headers()
                    self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))
                except Exception:
                    pass
        else:
            self.send_response(404)
            self.end_headers()
            
    def log_message(self, format, *args):
        # 将原生的 HTTP 日志引导进 logging 系统
        logging.info("🌐 " + format % args)

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    def handle_error(self, request, client_address):
        # 忽略控制台打印断连报警
        import sys
        exctype, value = sys.exc_info()[:2]
        if exctype is not None and (issubclass(exctype, ConnectionAbortedError) or issubclass(exctype, ConnectionResetError)):
            return
        HTTPServer.handle_error(self, request, client_address)

def run_server(port=5000):
    server_address = ('127.0.0.1', port)
    httpd = ThreadedHTTPServer(server_address, RequestHandler)
    logging.info(f"🚀 Starting Gemini Local HTTP Service on http://127.0.0.1:{port}")
    logging.info(f"⚙️ 当前日志等级: {logging.getLevelName(logging.getLogger().getEffectiveLevel())} (想要看全量 JSON 请求请带上 --debug 参数)")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logging.info("\n👋 Shutting down server...")
        httpd.server_close()

if __name__ == "__main__":
    if "test" in sys.argv:
        text = "わたし"
        print(json.dumps(analyze_text(text), ensure_ascii=False))
    else:
        run_server()
