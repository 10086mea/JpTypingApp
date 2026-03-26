#pragma once
#include <coroutine>
#include <exception>

// 一个极简但符合工业标准的 C++20 协程 Task 封装
template<typename T>
struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T value;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }
        std::suspend_never initial_suspend() { return {}; } // 创建后立即执行
        std::suspend_always final_suspend() noexcept { return {}; } // 结束后挂起，等待清理
        void unhandled_exception() { exception = std::current_exception(); }

        template<std::convertible_to<T> From>
        void return_value(From&& v) {
            value = std::forward<From>(v);
        }
    };

    handle_type coro;

    Task(handle_type h) : coro(h) {}
    ~Task() {
        if (coro) coro.destroy();
    }

    // 禁用拷贝，允许移动
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& t) noexcept : coro(t.coro) { t.coro = nullptr; }

    bool is_ready() const { return coro.done(); }

    T get_result() {
        if (coro.promise().exception)
            std::rethrow_exception(coro.promise().exception);
        return coro.promise().value;
    }
};

// 为 void 返回类型特化 Task
template<>
struct Task<void> {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::exception_ptr exception;

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception = std::current_exception(); }
        void return_void() {}
    };

    handle_type coro;

    Task(handle_type h) : coro(h) {}
    ~Task() {
        if (coro) coro.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& t) noexcept : coro(t.coro) { t.coro = nullptr; }

    bool is_ready() const { return coro.done(); }

    void get_result() {
        if (coro.promise().exception)
            std::rethrow_exception(coro.promise().exception);
    }
};