# JpTypingApp - 二次元 AI 日语学习伴侣

![Demo](media__1774894088499.png)

JpTypingApp 是一个结合了先进的大语言模型 (Gemini 2.5 Flash / Pro TTS) 和现代 C++ 图形渲染的极速本地桌面应用。其核心目标是为外语学习者打造一个集“沉浸代码级无痕查错”与“全自动语音二次元聊天”于一体的新时代工具。

## ❤️ 核心体验亮点

- **二次元美少女聊天视图 (Chat Mode)**
  - 你不仅可以自己定制她的“角色预设”和“口癖”，每当发送日文交流时，底层的 `google-genai` 会高速推理出日文语气的拟人化文本，并联动 `gemini-2.5-pro-preview-tts`，最终通过 C++ `SND_ASYNC` 异步为你送上原生语音播报。
- **丝滑 IDE 打字模式 (Typing Mode)**
  - 我们为你编写了类似 VSCode Copilot 的“幽灵预测高亮”。当防抖检测到你不敲键盘时，C++ 会分流出微服务协程拿到预测字串，使用 ImDrawList 将绿色的假名偷偷抹在你的真实输入框下方。
  - 按下 `Ctrl + Enter`：享受红名“病句”到绿字“更正”的无情划线处决，以及全中文的文法解析。
- **原生麦克风无缝听写 (WinRT Speech Recognition)**
  - 彻底摆脱粗糙的 UI，在 C++ 层下沉召唤 UWP API `Windows.Media.SpeechRecognition`。按下按钮，就可以一边说日语一边自动转写到输入框中。
- **工业级 C++20 协程并发引擎**
  - 使用双持锁令牌 (`Cancellation Token`) 保障防丢、断联和取消保护，打字再快，野指针崩溃 (UAF) 也无处遁形，帧率永远锁死最高！

## 🚀 快速启动指南

### 1. 配置云端引擎 (API Key)
请先在 `api_service` 文件夹中准备你的钥匙，新建或编辑 `config.json` 文件：
```json
{
    "API_KEY": "AIzaSy_填入你的谷歌_Gemini_Key_xxxxxxxx"
}
```

### 2. 部署极简轻量后台
为了避免每次按键唤醒冷启动 Python，我们采用了零延时的挂起后台模型：
```bash
cd api_service
pip install -r requirements.txt
python gemini_service.py
```
> 服务暴露在 `127.0.0.1:5000`，**请保持该黑框在后台长静默运行**。

### 3. 构建并运行 C++ 主程序
建议在具有 MSVC 环境的 Windows 平台展开编译。项目配置极其干净，第三方框架HttpLib以源码形式内嵌。imGUI需要拉取[imgui-module](https://github.com/stripe2933/imgui-module)到项目根目录下的/thirdparty/imgui，详见CMakeLists.txt
```bash
# 回到项目根目录
mkdir build
cd build
cmake ..
cmake --build .

# 启动！
.\Debug\JpTypingApp.exe
```


