#include "task.hpp"

namespace ComputerPlaysFactorio {

    SubTask *Task::GetSubTask() {
        std::scoped_lock lock(m_mutex);
        if (m_sub_tasks.empty()) return nullptr;
        return &m_sub_tasks.front();
    }

    void Task::PopSubTask() {
        std::scoped_lock lock(m_mutex);
        if (m_sub_tasks.empty()) return;
        m_sub_tasks.pop_front();
    }

    size_t Task::SubTaskCount() {
        std::scoped_lock lock(m_mutex);
        return m_sub_tasks.size();
    }

    void Task::QueueSubTask(const SubTask::Handler &handler) {
        std::scoped_lock lock(m_mutex);
        m_sub_tasks.emplace_back(handler);
        m_sub_task_cond.notify_all();
    }
}