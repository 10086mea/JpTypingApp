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
    std::shared_ptr<std::mutex> cancel_mutex;
    std::shared_ptr<bool> is_cancelled;

    struct SharedData {
        std::string result_json;
    };
    std::shared_ptr<SharedData> data;

    AsyncHttpPost(std::string text, std::shared_ptr<std::mutex> mut, std::shared_ptr<bool> flag)
        : text_input(text), cancel_mutex(mut), is_cancelled(flag) {
        data = std::make_shared<SharedData>();
    }

    bool await_ready() { return false; } // 总是挂起

    void await_suspend(std::coroutine_handle<> h) {
        // 深拷贝一份给 lambda，防止协程帧被销毁时触发 Access Violation
        std::string input_copy = text_input; 
        
        std::thread([input = std::move(input_copy), mut = cancel_mutex, flag = is_cancelled, d = data, h]() {
            httplib::Client cli("127.0.0.1", 5000);
            cli.set_connection_timeout(2, 0); // 2秒连接超时
            cli.set_read_timeout(30, 0); // 30秒读取超时
            
            json body = {{"text", input.c_str()}};
            if (auto res = cli.Post("/analyze", body.dump(), "application/json")) {
                if (res->status == 200) {
                    d->result_json = res->body;
                } else {
                    d->result_json = "{\"error\": \"HTTP Error " + std::to_string(res->status) + "\"}";
                }
            } else {
                d->result_json = "{\"error\": \"Failed to connect to Local Gemini Server\"}";
            }
            
            // 安全临界区：确保主线程在执行 m_activeTask 覆盖（进而销毁此协程）时，与此检查完全互斥
            std::lock_guard<std::mutex> lock(*mut);
            if (!(*flag)) {
                // 如果未被标记放弃，才进行恢复，避免在已被释放的栈上操作内存
                h.resume(); 
            }
        }).detach();
    }

    std::string await_resume() { return data->result_json; }
};

AppUI::AppUI() : m_isTyping(false), m_apiStatus("等待输入...") {
    m_inputText.resize(1024 * 16, '\0'); 
}

void AppUI::Render() {
    // 设置美化主题与全局排版
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.13f, 0.17f, 1.0f));
    
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
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    if (m_isTyping) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastInputTime) >= m_debounceDelay) {
            m_isTyping = false;
            OnInputReady(); 
        }
    }
}

void AppUI::DrawLeftPanel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.16f, 0.21f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    
    ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.55f, 0), true, ImGuiWindowFlags_MenuBar);
    
    if (ImGui::BeginMenuBar()) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "📝 日文输入区");
        ImGui::EndMenuBar();
    }
    
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_CallbackCompletion;
    
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        
        // 顶部状态栏
        if (m_apiStatus == "分析中...") {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "⏳ %s", m_apiStatus.c_str());
        } else {
            ImGui::TextDisabled("ℹ️ %s", m_apiStatus.c_str());
        }
        ImGui::Spacing();

        // 核心输入框
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        // 动态高度，留出底部空间显示补全提示
        float input_height = ImGui::GetContentRegionAvail().y - 45.0f;
        
        if (ImGui::InputTextMultiline("##Input", m_inputText.data(), m_inputText.capacity(), 
                                      ImVec2(-FLT_MIN, input_height), input_flags,
                                      [](ImGuiInputTextCallbackData* data) -> int {
                                          AppUI* app = (AppUI*)data->UserData;
                                          if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                                              // 注意此时外层已经有了 m_uiMutex 锁，不需要再次加锁
                                              if (!app->m_completion.empty()) {
                                                  data->InsertChars(data->CursorPos, app->m_completion.c_str());
                                                  app->m_completion.clear();
                                                  app->m_apiStatus = "✨ 补全已应用";
                                              }
                                          }
                                          return 0;
                                      }, this)) {
            m_lastInputTime = std::chrono::steady_clock::now();
            m_isTyping = true;
            m_completion.clear(); // 用户一打字，立刻清空旧的补全建议
            m_apiStatus = "等待输入停顿...";
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        // 底部内联补全提示 (Copilot 风格)
        if (!m_completion.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.6f, 1.0f)); // 强调绿色
            ImGui::Text("💡 按 [Tab] 键一键补全: %s", m_completion.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("💡 停顿 0.5 秒自动获取补全候选");
        }
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void AppUI::DrawRightPanel() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.19f, 0.25f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);
    
    if (ImGui::BeginMenuBar()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "🔍 语法诊断面板");
        ImGui::EndMenuBar();
    }
    
    std::vector<GrammarError> errors_copy;
    std::string status_copy;
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        errors_copy = m_errors;
        status_copy = m_apiStatus;
    }
    
    ImGui::Spacing();
    
    if (errors_copy.empty()) {
        if (status_copy == "分析中...") {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "正在等待 AI 专家检查...");
        } else if (status_copy == "等待输入停顿...") {
             ImGui::TextDisabled("...");
        } else if (status_copy.find("错误") != std::string::npos) {
             ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "API 调用失败，请检查服务。");
        } else {
             // 只有在分析完成且没有错误时才夸奖
             ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
             ImGui::TextWrapped("🎉 完美！目前的句子没有发现明显的语法错误。");
             ImGui::PopStyleColor();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "⚠️ 发现了 %d 处需要注意的地方：", (int)errors_copy.size());
        ImGui::Spacing();
        
        for (size_t i = 0; i < errors_copy.size(); ++i) {
            const auto& err = errors_copy[i];
            ImGui::PushID((int)i);
            
            // 为每个语法错误画一个高颜值的卡片
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.23f, 0.29f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            
            // 动态高度
            ImGui::BeginChild("ErrorCard", ImVec2(0, 110), true, ImGuiWindowFlags_NoScrollbar);
            
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "❌ 发现错误: %s", err.wrong_text.c_str());
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "✅ 修改建议: %s", err.correction.c_str());
            ImGui::Separator();
            
            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text("💡 解析: %s", err.explanation.c_str());
            ImGui::PopTextWrapPos();
            
            ImGui::EndChild();
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
            
            ImGui::Spacing();
        }
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void AppUI::OnInputReady() {
    std::string current_text;
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        current_text = m_inputText.c_str(); 
        m_apiStatus = "分析中...";
    }
    
    // 如果没有任何真正的字符，就不请求了
    if (current_text.empty() || current_text == "\n") {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        m_apiStatus = "等待输入...";
        return;
    }
    
    // 【协程安全】优雅地注销前一个仍在排队等待的协程模型
    // 防止底层 thread 在回调唤醒 (h.resume) 时，遇到 use-after-free
    if (m_cancelMutex && m_isCancelled) {
        std::lock_guard<std::mutex> lock(*m_cancelMutex);
        *m_isCancelled = true;
    }
    
    // 创建这一个全新请求的生命周期令牌
    m_cancelMutex = std::make_shared<std::mutex>();
    m_isCancelled = std::make_shared<bool>(false);
    
    // 启动协程并保持其句柄，旧的协程将在此被安全的销毁
    m_activeTask.emplace(FetchGemini(current_text, m_cancelMutex, m_isCancelled));
}

Task<void> AppUI::FetchGemini(std::string text, std::shared_ptr<std::mutex> mut, std::shared_ptr<bool> flag) {
    if (text.empty()) {
        co_return;
    }
    
    // 1. 发起异步请求，当前协程挂起，不阻塞主线程
    std::string response = co_await AsyncHttpPost{text, mut, flag};
    
    // 2. 协程在后台网络线程恢复，解析 JSON
    std::string new_completion;
    std::vector<GrammarError> new_errors;
    std::string new_status;
    
    try {
        json res_json = json::parse(response);
        if (res_json.contains("error")) {
            new_status = "[API 错误] " + res_json["error"].get<std::string>();
        } else {
            new_completion = res_json.value("completion", "");
            
            // LLM 有时候返回 None 或 null
            if (res_json.contains("grammar_errors") && res_json["grammar_errors"].is_array()) {
                for (const auto& err : res_json["grammar_errors"]) {
                    GrammarError ge;
                    ge.wrong_text = err.value("wrong_text", "");
                    ge.correction = err.value("correction", "");
                    ge.explanation = err.value("explanation", "");
                    new_errors.push_back(ge);
                }
            }
            new_status = "✨ 分析完成";
        }
    } catch (const std::exception& e) {
        new_status = "JSON 解析失败 (可能是网络或结构问题)";
    }
    
    // 3. 多线程安全地将结果推送到 UI 变量
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        // 如果用户在我们请求 API 期间内又修改了文本（开始了新的打字），我们不应该覆盖状态
        // 用一个简单的弱同步：检查打字状态
        if (!m_isTyping) {
            m_completion = new_completion;
            m_errors = std::move(new_errors);
            m_apiStatus = new_status;
        }
    }
}