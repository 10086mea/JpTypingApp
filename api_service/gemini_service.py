import sys
import os
import json
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
try:
    with open(config_path, "r", encoding="utf-8") as f:
        config = json.load(f)
    API_KEY = config.get("API_KEY", "")
except Exception as e:
    API_KEY = ""

URL = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={API_KEY}"

def analyze_text(text):
    prompt = f"""
You are an advanced Japanese language assistant.
The user is typing Japanese in a typing practice application.
Their current input is: '{text}'

Your tasks are:
1. Typing completion: Predict what the user is most likely trying to type next (a few words or characters to complete the sentence or phrase). Provide just the completion part, not the whole sentence. If the input is empty or invalid, provide an empty string for completion.
2. Grammar check: Analyze the currently input text. Find any grammatical errors, unnatural phrasing, or typos, and provide annotations. If there are no errors, return an empty list for grammar_errors.

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
    
    req = urllib.request.Request(URL, data=json.dumps(data).encode('utf-8'), headers={'Content-Type': 'application/json'})
    
    try:
        response = urllib.request.urlopen(req)
        response_body = response.read().decode('utf-8')
        result_json = json.loads(response_body)
        
        content_text = result_json.get('candidates', [{}])[0].get('content', {}).get('parts', [{}])[0].get('text', '{}')
        
        try:
            return json.loads(content_text.strip())
        except json.JSONDecodeError:
            if content_text.startswith("```json"):
                content_text = content_text[7:]
            if content_text.endswith("```"):
                content_text = content_text[:-3]
            return json.loads(content_text.strip())
        
    except Exception as e:
        return {"error": str(e), "completion": "", "grammar_errors": []}

class RequestHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/analyze':
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            
            try:
                request_json = json.loads(post_data.decode('utf-8'))
                text = request_json.get('text', '')
                
                result = analyze_text(text)
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.end_headers()
                self.wfile.write(json.dumps(result, ensure_ascii=False).encode('utf-8'))
            except ConnectionAbortedError:
                # Client disconnected before we could send the response
                pass
            except Exception as e:
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
            
    # Disable default logging to keep terminal clean
    def log_message(self, format, *args):
        pass

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
    print(f"Starting Gemini Local HTTP Service on http://127.0.0.1:{port}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.server_close()

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        text = "わたし"
        print(json.dumps(analyze_text(text), ensure_ascii=False))
    else:
        run_server()
