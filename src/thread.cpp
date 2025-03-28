#include "thread.hpp"

namespace ComputerPlaysFactorio {

bool ThreadPool::stop = true;
int ThreadPool::workingThread = 0;

std::mutex ThreadPool::queueMutex;
std::condition_variable ThreadPool::mutexCond;
std::vector<std::thread> ThreadPool::threads;
std::condition_variable ThreadPool::workingCond;
std::queue<std::function<void()>> ThreadPool::jobs;

void ThreadPool::Start(uint8_t threadCount) {
    threads.clear();
    for (uint8_t i = 0; i < threadCount; i++) {
        threads.emplace_back(std::thread(&ThreadPool::ThreadLoop));
    }
}

void ThreadPool::ThreadLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            mutexCond.wait(lock, [] {
                return !jobs.empty() || stop;
            });
            if (stop) return;
            job = jobs.front();
            jobs.pop();
            workingThread++;
        }
        job();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            workingThread--;
            if (!stop && jobs.empty() && workingThread == 0) {
                workingCond.notify_all();
            }
        }
    }
}

void ThreadPool::QueueJob(const std::function<void()>& job) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        jobs.push(job);
    }
    mutexCond.notify_one();
}

void ThreadPool::WaitAll() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        workingCond.wait(lock, [] {
            return jobs.empty() && workingThread == 0;
        });
    }
}

bool ThreadPool::Busy() {
    bool busy;
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        busy = !jobs.empty() || workingThread != 0;
    }
    return busy;
}

void ThreadPool::Stop() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    mutexCond.notify_all();
    for (std::thread &active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}
}