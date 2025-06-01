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
        static void Wait();
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

    class Waiter {
    public:
        bool Wait();

        void ForceUnlock();
        bool IsLocked();
        int LockCount();

    private:
        friend class WaiterLock;

        void Lock();
        void Unlock();

        int m_lockCount = 0;
        bool m_forced;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    };

    using SharedWaiter = std::shared_ptr<Waiter>;

    class WaiterLock {
    public:
        inline WaiterLock(Waiter &waiter) : m_waiter(&waiter) {
            waiter.Lock();
        }

        inline ~WaiterLock() {
            Unlock();
        }

        inline void Lock() {
            std::scoped_lock lock(m_mutex);
            if (m_locked) return;
            m_waiter->Lock();
            m_locked = true;
        }

        inline void Unlock() {
            std::scoped_lock lock(m_mutex);
            if (!m_locked) return;
            m_waiter->Unlock();
            m_locked = false;
        }

    private:
        bool m_locked = true;
        std::mutex m_mutex;
        Waiter *m_waiter;
    };
}