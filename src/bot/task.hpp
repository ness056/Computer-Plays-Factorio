#pragma once

#include "../factorio-API/factorio-API.hpp"

namespace ComputerPlaysFactorio {

    class SubTask {
    public:
        using Handler = std::function<void(FactorioInstance&)>;

        enum Type {
            NORMAL,
            TASK_END    // Marks the end of a task.
        };

        SubTask(const Handler &handler, Type type = NORMAL) :
            m_handler(handler), m_type(type) {}

        inline void Call(FactorioInstance &sub_task) const {
            m_handler(sub_task);
        }

        inline Type GetType() { return m_type; }
        
        inline void SetDescription(const std::string &str) {
            m_description = str;
        }

        inline const std::string &GetDescription() const {
            return m_description;
        }

    private:
        Handler m_handler;
        const Type m_type;
        std::string m_description;
    };

    class Task {
    public:
        Task(std::condition_variable &sub_task_cond) : m_sub_task_cond(sub_task_cond) {}

        void QueueSubTask(const SubTask::Handler&);
        SubTask *GetSubTask();
        void PopSubTask();
        size_t SubTaskCount();

    private:
        std::deque<SubTask> m_sub_tasks;
        std::mutex m_mutex;
        std::condition_variable &m_sub_task_cond;
    };
}