#include "thread.hpp"

namespace ComputerPlaysFactorio {

    bool ThreadPool::s_stop = true;
    int ThreadPool::s_working_thread = 0;

    std::mutex ThreadPool::s_queue_mutex;
    std::condition_variable ThreadPool::s_mutex_cond;
    std::vector<std::thread> ThreadPool::s_threads;
    std::condition_variable ThreadPool::s_working_cond;
    std::queue<std::function<void()>> ThreadPool::s_jobs;

    void ThreadPool::Start(uint8_t threadCount) {
        if (!s_stop) return;
        s_stop = false;

        s_threads.clear();
        for (uint8_t i = 0; i < threadCount; i++) {
            // According to cppreference the set_terminate should propagate to all threads,
            // but it seems MSVC does not follow the cpp standard...
            auto terminate = std::get_terminate();
            s_threads.emplace_back(&ThreadPool::ThreadLoop, terminate);
        }
    }

    void ThreadPool::ThreadLoop(terminate_handler terminate) {
        std::set_terminate(terminate);

        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(s_queue_mutex);
                s_mutex_cond.wait(lock, [] {
                    return !s_jobs.empty() || s_stop;
                });
                if (s_stop) return;
                job = s_jobs.front();
                s_jobs.pop();
                s_working_thread++;
            }
            job();
            {
                std::unique_lock<std::mutex> lock(s_queue_mutex);
                s_working_thread--;
                if (!s_stop && s_jobs.empty() && s_working_thread == 0) {
                    s_working_cond.notify_all();
                }
            }
        }
    }

    void ThreadPool::QueueJob(const std::function<void()>& job) {
        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            s_jobs.push(job);
        }
        s_mutex_cond.notify_one();
    }

    void ThreadPool::Wait() {
        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            s_working_cond.wait(lock, [] {
                return s_jobs.empty() && s_working_thread == 0;
            });
        }
    }

    bool ThreadPool::Busy() {
        bool busy;
        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            busy = !s_jobs.empty() || s_working_thread != 0;
        }
        return busy;
    }

    void ThreadPool::Stop() {
        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            s_stop = true;
        }
        s_mutex_cond.notify_all();
        for (std::thread &active_thread : s_threads) {
            active_thread.join();
        }
        s_threads.clear();
    }
}