#pragma once
#include <string>
#include <chrono>

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

private:
    std::string m_inputText;
    std::string m_outputText;

    // 防抖相关的状态机变量
    bool m_isTyping;
    std::chrono::steady_clock::time_point m_lastInputTime;
    const std::chrono::milliseconds m_debounceDelay{500}; 
};