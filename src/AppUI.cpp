#include "AppUI.h"
#include "imgui.h"
#include <iostream>

AppUI::AppUI() : m_isTyping(false) {
    // 预分配足够的空间，减少输入过程中的高频堆内存重分配
    m_inputText.resize(1024 * 16, '\0'); 
    m_outputText = "等待输入...\n";
}

void AppUI::Render() {
    // 设置全屏窗口作为主容器
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin("MainWorkspace", nullptr, window_flags);

    // 左右分栏布局
    DrawLeftPanel();
    ImGui::SameLine();
    DrawRightPanel();

    ImGui::End();

    // ==========================================
    // 核心防抖逻辑 (Debounce State Machine)
    // ==========================================
    if (m_isTyping) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastInputTime) >= m_debounceDelay) {
            m_isTyping = false;
            OnInputReady(); // 500ms 内无新输入，触发！
        }
    }
}

void AppUI::DrawLeftPanel() {
    // 左侧面板占一半宽度
    ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), true);
    ImGui::Text("日文输入区 (实时)");
    ImGui::Separator();

    // 监听文本变化
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AllowTabInput;
    if (ImGui::InputTextMultiline("##Input", m_inputText.data(), m_inputText.capacity(), 
                                  ImVec2(-FLT_MIN, -FLT_MIN), input_flags)) {
        // 只要有任何字符变动，重置定时器
        m_lastInputTime = std::chrono::steady_clock::now();
        m_isTyping = true;
    }
    
    ImGui::EndChild();
}

void AppUI::DrawRightPanel() {
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    ImGui::Text("DeepSeek 预测与纠错");
    ImGui::Separator();
    
    // 右侧仅做展示，自动换行
    ImGui::TextWrapped("%s", m_outputText.c_str());
    
    ImGui::EndChild();
}

void AppUI::OnInputReady() {
    // 这里暂时用打印代替，后续模块我们会在这里接入 C++20 协程网络请求
    std::cout << "[架构日志] 触发 OnInputReady. 提取文本发送给 DeepSeek..." << std::endl;
    m_outputText = "正在分析输入: \n" + std::string(m_inputText.c_str());
}