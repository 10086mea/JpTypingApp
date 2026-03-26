#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // 必须在 ImGui 之后引入
#include <iostream>
#include <cstdlib>

#include "AppUI.h"
#ifdef _WIN32
#include <windows.h>
#endif

// GLFW 错误回调，遇到底层上下文崩溃时方便排查
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "[GLFW Error] " << error << ": " << description << std::endl;
}

int main(int, char**) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    // 1. 初始化 GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return EXIT_FAILURE;
    }

    // 决定 GL + GLSL 版本 (这里使用通用的 OpenGL 3.0 / GLSL 130)
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // 2. 创建窗口与 OpenGL 上下文
    GLFWwindow* window = glfwCreateWindow(1280, 720, "JpTypingApp - C++20 架构实战", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // 开启 VSync (垂直同步)，限制帧率防止 CPU/GPU 满载跑 1000+ FPS

    // 3. 初始化 Dear ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 允许键盘导航
// --- 新增：加载支持中日文的系统字体 ---
    // 使用 Windows 自带的微软雅黑 (包含大量中日文汉字与假名)
    // 18.0f 是字体大小，GetGlyphRangesChineseFull() 包含了完整的中文和日文常用字形
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "c:\\Windows\\Fonts\\msyh.ttc", 
        18.0f, 
        nullptr, 
        io.Fonts->GetGlyphRangesChineseFull()
    );
    if (font == nullptr) {
        std::cerr << "[警告] 字体加载失败，将使用默认全英文备用字体！" << std::endl;
    }
    // 设置暗色主题 (大厂标配)
    ImGui::StyleColorsDark();

    // 4. 初始化 ImGui 后端
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ==========================================
    // 实例化我们核心的 UI 业务层
    // ==========================================
    AppUI appUI; 

    // 5. 主渲染循环 (Main Render Loop)
    while (!glfwWindowShouldClose(window)) {
        // 轮询操作系统事件 (键盘、鼠标、窗口关闭等)
        glfwPollEvents();

        // 启动 ImGui 新一帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ----------------------------------------
        // 执行我们的业务逻辑与防抖渲染
        appUI.Render(); 
        // ----------------------------------------

        // 渲染 ImGui 生成的 DrawData 到屏幕
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        // 清屏颜色 (深灰蓝)
        glClearColor(0.15f, 0.16f, 0.21f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 交换双缓冲
        glfwSwapBuffers(window);
    }

    // 6. 优雅清理资源
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}