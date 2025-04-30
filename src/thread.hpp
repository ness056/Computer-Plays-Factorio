#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace ComputerPlaysFactorio {

class ThreadPool {
public:
    ThreadPool() = delete;

    static void Start(uint8_t threadCount);
    static void QueueJob(const std::function<void()>& job);
    static void WaitAll();
    static void Stop();
    static bool Busy();

private:
    static void ThreadLoop();

    static bool s_stop;
    static std::mutex s_queueMutex;
    static std::condition_variable s_mutexCond;
    static std::vector<std::thread> s_threads;
    static int s_workingThread;
    static std::condition_variable s_workingCond;
    static std::queue<std::function<void()>> s_jobs;
};
}