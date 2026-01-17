#pragma once

#include <queue>
#include <coroutine>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace ComputerPlaysFactorio {

#define CHECKPOINT co_await std::suspend_always{};

    struct Task {
    public:
        struct promise_type {
            int priority = 0;

            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }

            Task get_return_object() noexcept {
                return std::coroutine_handle<promise_type>::from_promise(*this);
            }

            void return_void() noexcept {}
            void unhandled_exception() {}
        };

        using handle_type = std::coroutine_handle<promise_type>;

        Task() = default;
        Task(handle_type handle) : m_handle{handle} {}

        inline bool Done() const { return !m_handle || m_handle.done(); }
        inline void Resume() { if (m_handle) m_handle.resume(); }
        inline int Priority() const { return m_handle ? m_handle.promise().priority : 0; }
        inline void SetPriority(int priority) { if (m_handle) m_handle.promise().priority = priority; }
        inline void Destroy() { if (m_handle) m_handle.destroy(); }
        
        bool operator<(const Task &other) const {
            return Priority() < other.Priority();
        }

    private:
        handle_type m_handle;
    };

    class Scheduler {
    public:
        void Enqueue(std::function<Task()> task, int priority = 0);
        void EnqueueImmediate(std::function<void()> callback);

        void Loop();

        inline void Stop() {
            m_stop = true;
        }

    private:
        std::priority_queue<Task> m_tasks;
        std::queue<std::function<void()>> m_callbacks;
        std::mutex m_mutex;
        std::condition_variable m_cond;

        std::atomic<bool> m_stop = false;
    };
}