#include "thread.hpp"

namespace ComputerPlaysFactorio {

bool ThreadPool::s_stop = true;
int ThreadPool::s_workingThread = 0;

std::mutex ThreadPool::s_queueMutex;
std::condition_variable ThreadPool::s_mutexCond;
std::vector<std::thread> ThreadPool::s_threads;
std::condition_variable ThreadPool::s_workingCond;
std::queue<std::function<void()>> ThreadPool::s_jobs;

void ThreadPool::Start(uint8_t threadCount) {
    if (!s_stop) return;
    s_stop = false;

    s_threads.clear();
    for (uint8_t i = 0; i < threadCount; i++) {
        s_threads.emplace_back(std::thread(&ThreadPool::ThreadLoop));
    }
}

void ThreadPool::ThreadLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(s_queueMutex);
            s_mutexCond.wait(lock, [] {
                return !s_jobs.empty() || s_stop;
            });
            if (s_stop) return;
            job = s_jobs.front();
            s_jobs.pop();
            s_workingThread++;
        }
        job();
        {
            std::unique_lock<std::mutex> lock(s_queueMutex);
            s_workingThread--;
            if (!s_stop && s_jobs.empty() && s_workingThread == 0) {
                s_workingCond.notify_all();
            }
        }
    }
}

void ThreadPool::QueueJob(const std::function<void()>& job) {
    {
        std::unique_lock<std::mutex> lock(s_queueMutex);
        s_jobs.push(job);
    }
    s_mutexCond.notify_one();
}

void ThreadPool::WaitAll() {
    {
        std::unique_lock<std::mutex> lock(s_queueMutex);
        s_workingCond.wait(lock, [] {
            return s_jobs.empty() && s_workingThread == 0;
        });
    }
}

bool ThreadPool::Busy() {
    bool busy;
    {
        std::unique_lock<std::mutex> lock(s_queueMutex);
        busy = !s_jobs.empty() || s_workingThread != 0;
    }
    return busy;
}

void ThreadPool::Stop() {
    {
        std::unique_lock<std::mutex> lock(s_queueMutex);
        s_stop = true;
    }
    s_mutexCond.notify_all();
    for (std::thread &active_thread : s_threads) {
        active_thread.join();
    }
    s_threads.clear();
}
}