#include "scheduler.hpp"

namespace ComputerPlaysFactorio {

    void Scheduler::Enqueue(std::function<Task()> fn, int priority) {
        auto task = fn();
        task.SetPriority(priority);
        {
            std::scoped_lock lock(m_mutex);
            m_tasks.emplace(std::move(task));
        }
        m_cond.notify_one();
    }

    void Scheduler::EnqueueImmediate(std::function<void()> callback) {
        {
            std::scoped_lock lock(m_mutex);
            m_callbacks.emplace(callback);
        }
        m_cond.notify_one();
    }

    void Scheduler::Loop() {
        m_stop = false;
        while (!m_stop.load()) {
            Task task;
            {
                std::unique_lock lock(m_mutex);
                m_cond.wait(lock, [&] {
                    return m_stop.load() || !m_tasks.empty() || !m_callbacks.empty();
                });

                if (m_stop.load()) break;

                if (!m_tasks.empty()) {
                    task = std::move(m_tasks.top());
                    m_tasks.pop();
                }
            }

            while (!m_callbacks.empty()) {
                std::function<void()> callback;
                {
                    std::scoped_lock lock(m_mutex);
                    callback = std::move(m_callbacks.front());
                    m_callbacks.pop();
                }
                callback();
            }

            if (!task.Done()) {
                task.Resume();
            }

            if (task.Done()) {
                task.Destroy();
            } else {
                std::scoped_lock lock(m_mutex);
                m_tasks.push(std::move(task));
            }
        }

        std::scoped_lock lock(m_mutex);
        while (!m_tasks.empty()) {
            m_tasks.pop();
        }
    }
}