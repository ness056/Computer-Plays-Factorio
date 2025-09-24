#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>

namespace ComputerPlaysFactorio {

    class ThreadPool {
    public:
        ThreadPool() = delete;

        static void Start(uint8_t thread_count);
        static void QueueJob(const std::function<void()>& job);
        static void Wait();
        static void Stop();
        static bool Busy();

    private:
        static void ThreadLoop(terminate_handler);

        static bool s_stop;
        static std::mutex s_queue_mutex;
        static std::condition_variable s_mutex_cond;
        static std::vector<std::thread> s_threads;
        static int s_working_thread;
        static std::condition_variable s_working_cond;
        static std::queue<std::function<void()>> s_jobs;
    };
}