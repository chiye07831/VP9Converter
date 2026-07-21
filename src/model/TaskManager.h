#pragma once

#include <vector>
#include <memory>
#include "Task.h"

class TaskManager
{
public:
    Task* add();
    void remove(int index);
    Task* current();
    const Task* current() const;
    int currentIndex() const { return m_currentIndex; }
    void select(int index);
    int count() const { return static_cast<int>(m_tasks.size()); }
    Task* get(int index);
    const Task* get(int index) const;

private:
    std::vector<std::unique_ptr<Task>> m_tasks;
    int m_currentIndex = -1;
};
