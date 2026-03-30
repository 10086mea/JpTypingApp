#include <iostream>
#include <string>

// 包含 C++/WinRT 基础和语音识别相关头文件
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.SpeechRecognition.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::SpeechRecognition;

int main() {
    try {
        // 1. 初始化多线程 COM 单元环境 (Windows Runtime 的底层基石)
        init_apartment();

        std::cout << "COM Apartment initialized. Creating SpeechRecognizer..." << std::endl;
        
        // 2. 实例化语音识别器对象
        SpeechRecognizer recognizer;
        
        std::cout << "Compiling constraints (default dictation grammar)..." << std::endl;
        // 3. 编译默认系统听写语法约束
        auto compileTask = recognizer.CompileConstraintsAsync();
        compileTask.get(); // 阻塞等待协程完成
        
        std::cout << "Constraints compiled! Start speaking now..." << std::endl;
        
        // 4. 调用 RecognizeAsync 开始无 UI 的监听（会自动采集麦克风短句）
        auto result = recognizer.RecognizeAsync().get();
        
        // 5. 打印捕获的文本
        std::wcout << L"Recognized Text: " << result.Text().c_str() << std::endl;
        std::cout << "Status: " << (int)result.Status() << std::endl;

        return 0;
    } catch (winrt::hresult_error const& ex) {
        // C++/WinRT 标准异常捕获
        std::wcout << L"WinRT Error: " << ex.code() << L" - " << ex.message().c_str() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Error: " << e.what() << std::endl;
        return 2;
    }
}
