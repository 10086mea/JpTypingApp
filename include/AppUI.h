#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <optional>
#include <any>
#include "CoroTask.h"

struct GrammarError {
    std::string wrong_text;
    std::string correction;
    std::string explanation;
};

class AppUI {
public:
    AppUI();
    ~AppUI() = default;

    // 禁用拷贝与移动，保证单例或生命周期稳定
    AppUI(const AppUI&) = delete;
    AppUI& operator=(const AppUI&) = delete;

    void Render(); // 在 ImGui 主循环中每帧调用

private:
    void DrawLeftPanel();
    void DrawRightPanel();
    void DrawChatUI();

    void OnInputReady(); // 防抖触发后的核心业务函数 (补全)
    void OnGrammarCheckTriggered(); // 手动触发语法检测
    Task<void> FetchGemini(std::string text, std::string mode, std::shared_ptr<std::mutex> cancelMutex, std::shared_ptr<bool> isCancelled);
    Task<void> RecognizeSpeechAsync(std::shared_ptr<bool> isCancelled);
    Task<void> FetchChatAndTTS(std::string text, std::string system_prompt, std::shared_ptr<std::mutex> cancelMutex, std::shared_ptr<bool> isCancelled);

private:
    std::string m_inputText;
    
    // AI 结果存储
    std::string m_completion;
    std::vector<GrammarError> m_errors;
    std::string m_apiStatus; // 状态提示，如"等待输入"、"分析中..."、"API错误"等

    std::mutex m_uiMutex; // 保护多线程下的 UI 数据

    // 协程生命周期/多线程竞态安全令牌
    std::shared_ptr<std::mutex> m_cancelMutex;
    std::shared_ptr<bool> m_isCancelled;

    // 防抖相关的状态机变量
    bool m_isTyping;
    bool m_isRecording;
    std::chrono::steady_clock::time_point m_lastInputTime;
    int m_debounceDelayMs{1000}; 

    // 保存正在运行的协程任务（专注 Gemini 文本大模型）
    std::optional<Task<void>> m_activeTask;
    
    // 独立维护语音识别框架，使其与文本补全共存不再冲突打断
    std::optional<Task<void>> m_speechTask;
    std::shared_ptr<std::mutex> m_speechCancelMutex;
    std::shared_ptr<bool> m_speechCancelled;
    std::any m_speechRecognizer;
    int m_speechTimeoutMs{3000};

    // Chat UI variables
    enum class AppMode { Typing, Chat };
    AppMode m_currentMode = AppMode::Typing;

    struct ChatMessage {
        std::string role;
        std::string text;
    };
    std::vector<ChatMessage> m_chatHistory;
    std::string m_systemPrompt = "你是一个热爱二次元的活泼日本女高中生，你把我当做崇拜的前辈，在回复时语气元气满满，并且常用「〜だよね」「〜かな」等语气词结尾。请保持回复简短自然。";
    std::string m_chatInputText;
    
    std::optional<Task<void>> m_chatTask;
    std::shared_ptr<std::mutex> m_chatCancelMutex;
    std::shared_ptr<bool> m_chatCancelled;
};