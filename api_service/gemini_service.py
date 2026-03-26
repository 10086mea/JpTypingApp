import sys
import os
import json
import urllib.request
import urllib.error

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
            "temperature": 0.2, # Keep formatting strict and deterministic
            "responseMimeType": "application/json" # Enforce strict JSON
        }
    }
    
    req = urllib.request.Request(URL, data=json.dumps(data).encode('utf-8'), headers={'Content-Type': 'application/json'})
    
    try:
        response = urllib.request.urlopen(req)
        response_body = response.read().decode('utf-8')
        result_json = json.loads(response_body)
        
        # Extract the actual text from the Gemini response structure
        content_text = result_json.get('candidates', [{}])[0].get('content', {}).get('parts', [{}])[0].get('text', '{}')
        
        try:
            return json.loads(content_text.strip())
        except json.JSONDecodeError:
            # Fallback parsing in case it didn't strictly follow JSON formatting
            if content_text.startswith("```json"):
                content_text = content_text[7:]
            if content_text.endswith("```"):
                content_text = content_text[:-3]
            return json.loads(content_text.strip())
        
    except Exception as e:
        return {"error": str(e), "completion": "", "grammar_errors": []}

if __name__ == "__main__":
    text = ""
    # Support both command line argument and stdin
    if len(sys.argv) > 1:
        text = sys.argv[1]
    
    if not text:
        # If no argument, try to read from stdin (timeout after waiting to prevent hang)
        import select
        if select.select([sys.stdin,],[],[],0.0)[0]:
            text = sys.stdin.read().strip()
        
    result = analyze_text(text)
    print(json.dumps(result, ensure_ascii=False))
