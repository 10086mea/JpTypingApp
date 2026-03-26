#include "AppUI.h"
#include "imgui.h"
#include <iostream>
#include <thread>

// Header-only libraries
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

// 自定义的 C++20 Awaiter，用于在后台线程发起 HTTP 请求
struct AsyncHttpPost {
    std::string text_input;
    std::string result_json;

    bool await_ready() { return false; } // 总是挂起

    void await_suspend(std::coroutine_handle<> h) {
        // 在新线程中执行网络请求
        std::thread([this, h]() {
            httplib::Client cli("127.0.0.1", 5000);
            cli.set_connection_timeout(2, 0); // 2秒连接超时
            cli.set_read_timeout(30, 0); // 30秒读取超时 (Gemini 响应可能较慢)
            
            json body = {{"text", text_input}};
            if (auto res = cli.Post("/analyze", body.dump(), "application/json")) {
                if (res->status == 200) {
                    result_json = res->body;
                } else {
                    result_json = "{\"error\": \"HTTP Error " + std::to_string(res->status) + "\"}";
                }
            } else {
                result_json = "{\"error\": \"Failed to connect to Local Gemini Server (Is it running?)\"}";
            }
            
            // 注意：恢复协程。协程其余部分将在这个后台线程继续执行！
            h.resume(); 
        }).detach();
    }

    std::string await_resume() { return result_json; }
};

AppUI::AppUI() : m_isTyping(false) {
    m_inputText.resize(1024 * 16, '\0'); 
    m_outputText = "等待输入...\n";
}

void AppUI::Render() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin("MainWorkspace", nullptr, window_flags);
    DrawLeftPanel();
    ImGui::SameLine();
    DrawRightPanel();
    ImGui::End();

    if (m_isTyping) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastInputTime) >= m_debounceDelay) {
            m_isTyping = false;
            OnInputReady(); 
        }
    }
}

void AppUI::DrawLeftPanel() {
    ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0), true);
    ImGui::Text("日文输入区 (实时) - 输入停顿0.5秒后自动请求");
    ImGui::Separator();
    
    // UI 线程加锁保护
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AllowTabInput;
        // 注意：ImGui 修改的是 m_inputText.data() 的内容
        if (ImGui::InputTextMultiline("##Input", m_inputText.data(), m_inputText.capacity(), 
                                      ImVec2(-FLT_MIN, -FLT_MIN), input_flags)) {
            m_lastInputTime = std::chrono::steady_clock::now();
            m_isTyping = true;
        }
    }
    
    ImGui::EndChild();
}

void AppUI::DrawRightPanel() {
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    ImGui::Text("AI 预测与纠错 (Gemini 2.5 Flash)");
    ImGui::Separator();
    
    std::string output_copy;
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        output_copy = m_outputText;
    }
    
    ImGui::TextWrapped("%s", output_copy.c_str());
    
    ImGui::EndChild();
}

void AppUI::OnInputReady() {
    std::string current_text;
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        current_text = m_inputText.c_str(); 
        m_outputText = "正在分析输入: \n" + current_text + "\n(请求API中...)";
    }
    
    // 启动协程并保持其句柄，避免被提前销毁
    m_activeTask.emplace(FetchGemini(current_text));
}

Task<void> AppUI::FetchGemini(std::string text) {
    if (text.empty()) {
        co_return;
    }
    
    // 1. 发起异步请求，当前协程挂起，不阻塞主线程
    std::string response = co_await AsyncHttpPost{text, ""};
    
    // 2. 协程在后台网络线程恢复，解析 JSON
    std::string formatted_output;
    try {
        json res_json = json::parse(response);
        if (res_json.contains("error")) {
            formatted_output = "[Error] " + res_json["error"].get<std::string>();
        } else {
            std::string completion = res_json.value("completion", "");
            formatted_output = "【预测补全】\n" + completion + "\n\n";
            
            if (res_json.contains("grammar_errors") && res_json["grammar_errors"].is_array()) {
                formatted_output += "【语法检查】\n";
                auto errors = res_json["grammar_errors"];
                if (errors.empty()) {
                    formatted_output += "-> 没有发现明显的语法错误！太棒了！\n";
                } else {
                    for (const auto& err : errors) {
                        std::string wrong = err.value("wrong_text", "");
                        std::string fix = err.value("correction", "");
                        std::string exp = err.value("explanation", "");
                        formatted_output += "- 错误: " + wrong + "\n";
                        formatted_output += "  建议: " + fix + "\n";
                        formatted_output += "  解释: " + exp + "\n\n";
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        formatted_output = "JSON 解析失败: " + std::string(e.what()) + "\nRaw: " + response;
    }
    
    // 3. 多线程安全地将结果推送到 UI 变量
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        m_outputText = formatted_output;
    }
}