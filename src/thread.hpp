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
    static void Start(uint8_t threadCount);
    static void QueueJob(const std::function<void()>& job);
    static void WaitAll();
    static void Stop();
    static bool Busy();

private:
    static void ThreadLoop();

    static bool stop;
    static std::mutex queueMutex;
    static std::condition_variable mutexCond;
    static std::vector<std::thread> threads;
    static int workingThread;
    static std::condition_variable workingCond;
    static std::queue<std::function<void()>> jobs;
};
}