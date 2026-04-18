// Shared background worker pool used by renderer and asset systems.
#include "Engine/Core/JobSystem.h"

#include <algorithm>
#include <stdexcept>

namespace
{
    std::size_t ResolveDefaultWorkerCount()
    {
        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        if (hardwareThreads <= 1U)
        {
            return 1;
        }

        // Leave one thread for the main loop when hardware concurrency is available.
        return static_cast<std::size_t>(hardwareThreads - 1U);
    }
}

JobSystem& JobSystem::Get()
{
    static JobSystem instance;
    return instance;
}

JobSystem::JobSystem()
    : JobSystem(ResolveDefaultWorkerCount())
{
}

JobSystem::JobSystem(std::size_t workerCount)
{
    StartWorkers(std::max<std::size_t>(workerCount, 1));
}

JobSystem::~JobSystem()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stopping = true;
    }
    m_Condition.notify_all();

    for (std::thread& worker : m_Workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

std::size_t JobSystem::GetWorkerCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Workers.size();
}

void JobSystem::Enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Stopping)
        {
            throw std::runtime_error("Cannot submit job after JobSystem shutdown.");
        }

        m_Tasks.push(std::move(task));
    }

    m_Condition.notify_one();
}

void JobSystem::StartWorkers(std::size_t workerCount)
{
    m_Workers.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index)
    {
        m_Workers.emplace_back([this]() {
            WorkerLoop();
        });
    }
}

void JobSystem::WorkerLoop()
{
    for (;;)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Condition.wait(lock, [this]() {
                return m_Stopping || !m_Tasks.empty();
            });

            if (m_Stopping && m_Tasks.empty())
            {
                return;
            }

            task = std::move(m_Tasks.front());
            m_Tasks.pop();
        }

        task();
    }
}
