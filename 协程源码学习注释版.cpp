// 这一份文件是专门为你（没有任何 C++ 协程基础）准备的面试级源码精读版合集。
// 原本它被拆散在 include/CoroTask.h 和 src/AppUI.cpp 里，为了你突击学习，
// 我将它们合并成了这一份带保姆级汉化注释的“假 C++ 文件”，供你从头到尾走过整个协程大纲。

#include <coroutine>
#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include <string>

// =========================================================================
// 第一部分：Promise Type (承诺对象) 和 Task (返回值)
// 只有定义了这套黑科技模板，你才能把一个标有 co_await/co_return 的函数作为协程编译下去！
// （在本项目中对应 include/CoroTask.h）
// =========================================================================

// 这就是协程返回的空壳盒子：Task 类型
template<typename T = void>
struct Task {
    
    // 【核心黑魔法】：只要返回值为 Task 的函数一旦内部含有 co_xxxx 关键字，
    // C++ 编译器就会在背后自动寻找名为 `promise_type` 的嵌套类去掌控整个生命周期。
    struct promise_type {
        
        // 1. 协程被调用时，通过这行代码创建外壳返还给主调用者（比如外层的 UI 主线程）
        Task get_return_object() {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        // 2. 协程刚生出来时要在堆区挂起吗？（我们选择不挂机，直接开始冲：suspend_never）
        std::suspend_never initial_suspend() { return {}; }
        
        // 3. 协程所有的代码 (包括子线程里的业务) 全都 co_return 执行完销毁时，我们要挂起吗？
        // （通常在最后销毁时不挂起，选 suspend_never，让系统自己清空那块堆内存）
        std::suspend_never final_suspend() noexcept { return {}; }
        
        // 4. 用户代码写 co_return; 时触发
        void return_void() {} 

        // 5. 协程执行崩溃了抛出异常 (比如 json 转换爆炸了)，兜底接住
        void unhandled_exception() {}
    };

    // ----------- Task 本身的类方法 -----------
    // 把上帝给我们的遥控器存起来
    std::coroutine_handle<promise_type> coro;
    
    // 初始化构造时拿着遥控器
    explicit Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    
    // 【生死攸关的清理】：这块代码决定了我们的取消逻辑！
    // 当 AppUI::OnGrammarCheckTriggered() 里执行 m_activeTask.emplace() 时：
    // 新的 Task 把旧的挤掉，引发旧的 Task 触发这个析构函数！
    ~Task() {
        if (coro) coro.destroy(); // 毁天灭地的遥控器，按下去就把外包的这块堆空间连带变量全炸了！
    }
};

// =========================================================================
// 第二部分：Awaiter (等待体) 和 线程剥离并发解耦
// 负责具体执行后台耗时几十秒的 API 调用。
// 这是在向 C++ 编译器解释如何处理 "co_await AsyncHttpPost"
// （在本项目中对应 src/AppUI.cpp）
// =========================================================================

struct AsyncHttpPost {
    // 这里传入的用户输入和 Cancellation Token 安全锁，全部复制给 lambda
    std::string text_input;
    std::string req_mode;
    std::shared_ptr<std::mutex> cancel_mutex;
    std::shared_ptr<bool> is_cancelled;

    // 因为 lambda 后台线程和本协程堆栈生命周期解耦，返回值必须用安全的 shared_ptr 指向同一块堆区
    struct SharedData {
        std::string result_json;
    };
    std::shared_ptr<SharedData> data;

    AsyncHttpPost(std::string text, std::string mode, std::shared_ptr<std::mutex> mut, std::shared_ptr<bool> flag)
        : text_input(text), req_mode(mode), cancel_mutex(mut), is_cancelled(flag) {
        data = std::make_shared<SharedData>();
    }

    // Awaiter 的三个必写函数之 1：
    // true = 秒返回，直接往下跑； false = 我要挂机，请编译器把 CPU 交给外面的主线程！
    bool await_ready() { return false; } 

    // Awaiter 的三个必写函数之 2：
    // 当协程被编译器冻结在内存（堆）上的这一极短暂的瞬间被调用。
    // h 是被冻结的协程残骸的遥控器。
    void await_suspend(std::coroutine_handle<> h) {
        
        // 【面试加分细节】：把这几个基础类型全部深拷贝！因为一旦主线程抛弃了这个挂起的 Task，
        // 整个 AsyncHttpPost 的内部 this 作用域空间就会被 ~Task 无情扬掉！
        // 如果这里用 lambda 的 [this, h] 去捕获，后台线程会直接炸烂 (Use-After-Free 内存非法访问)！
        std::string input_copy = text_input; 
        std::string mode_copy = req_mode;
        
        // 甩开膀子：新开一条线程，将大耗时工作抛入系统后台！
        std::thread([input = std::move(input_copy), mode = std::move(mode_copy), mut = cancel_mutex, flag = is_cancelled, d = data, h]() {
            
            // ... 想象这里发送了耗时极长（15秒）的阻塞式 HTTP 请求 ...
            // json response = Httplib::Post("/analyze", input);
            // d->result_json = response;
            
            // ============ 万分凶险的临界区 ============ 
            // 此时后台网络拿到了结果！在它按下遥控器 `h.resume()` 唤醒协程继续计算之前，必须看看“主线程老爷”是不是早就翻脸把旧协程给清零了！
            std::lock_guard<std::mutex> lock(*mut);
            if (!(*flag)) {
                // 如果旗帜 flag 没有被主线程置为 true 放弃，我们才按下恢复键
                h.resume(); // <== 这！就是整个协程体系里最震撼的魔法：代码会从 co_await 这一行“穿越回魂”，继续在那条后台线程运行！
            }
            // 如果主线程置为 true 了，线程就走到末尾自己释放了，什么都不会发生，也不会内存崩溃，这就是优雅！
            
        }).detach(); // 强行和主线程脱离父子关系
    }

    // Awaiter 的三个必写函数之 3：
    // 当有人按下了 `h.resume()`，协程复燃重生后，第一句调用的就是这行。
    // 它的返回值就相当于这句 `std::string res = co_await ...` 等号右边的结果。
    std::string await_resume() { return data->result_json; }
};

// =========================================================================
// 第三部分：串起前两者的业务逻辑（调用者）
// =========================================================================

// 这是我们 AppUI.cpp 中的主业务函数，也是真正的“被修饰的协程”
Task<void> AppUI::FetchGemini(std::string text, std::string mode, std::shared_ptr<std::mutex> mut, std::shared_ptr<bool> flag) {
    if (text.empty()) {
        co_return;
    }
    
    // 【第一阶段】：主线程大模大样地走到这里... 
    // 遇到 co_await，编译器把它踢出去！UI 主线程退场（拿到了空壳 Task），屏幕不卡顿继续渲染去了！
    
    std::string response = co_await AsyncHttpPost{text, mode, mut, flag};
    
    // 【第二阶段】：15秒后！协程复活！
    // 此时此刻这行代码，其实已经是在上面剥离的那个 std::thread 后台子线程 里偷偷狂奔了！
    // 整个栈（局部变量）神奇地被保留了下来。
    
    std::string new_completion;
    
    try {
        json res_json = json::parse(response);
        new_completion = res_json.value("completion", "");
    } catch (...) {
        // ...
    }
    
    // 更新界面（因为我们在后台线程，所以要跨线程锁 m_uiMutex 给界面的字符串赋值）
    {
        std::lock_guard<std::mutex> lock(m_uiMutex);
        m_completion = new_completion;
    }
    
    // 【第三阶段】：结束
    // 遇到 co_return，调用底层 ~Task，把堆清理得干干净净
    co_return;
}
