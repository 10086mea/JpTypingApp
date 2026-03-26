#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <optional>
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
    void OnInputReady(); // 防抖触发后的核心业务函数
    Task<void> FetchGemini(std::string text, std::shared_ptr<std::mutex> cancelMutex, std::shared_ptr<bool> isCancelled);

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
    std::chrono::steady_clock::time_point m_lastInputTime;
    const std::chrono::milliseconds m_debounceDelay{500}; 

    // 保存正在运行的协程任务
    std::optional<Task<void>> m_activeTask;
};