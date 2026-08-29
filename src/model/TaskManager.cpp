#include "TaskManager.h"

Task* TaskManager::add()
{
    auto task = std::make_unique<Task>();
    task->outputName = "task" + std::to_string(m_tasks.size() + 1) + ".webm";
    Task* ptr = task.get();
    m_tasks.push_back(std::move(task));
    if (m_currentIndex < 0)
        m_currentIndex = 0;
    return ptr;
}

void TaskManager::remove(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tasks.size()))
        return;

    m_tasks.erase(m_tasks.begin() + index);

    if (m_tasks.empty())
    {
        m_currentIndex = -1;
    }
    else if (m_currentIndex >= static_cast<int>(m_tasks.size()))
    {
        m_currentIndex = static_cast<int>(m_tasks.size()) - 1;
    }
}

Task* TaskManager::current()
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_tasks.size()))
        return nullptr;
    return m_tasks[m_currentIndex].get();
}

const Task* TaskManager::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_tasks.size()))
        return nullptr;
    return m_tasks[m_currentIndex].get();
}

void TaskManager::select(int index)
{
    if (index >= 0 && index < static_cast<int>(m_tasks.size()))
        m_currentIndex = index;
}

Task* TaskManager::get(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tasks.size()))
        return nullptr;
    return m_tasks[index].get();
}

const Task* TaskManager::get(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_tasks.size()))
        return nullptr;
    return m_tasks[index].get();
}
