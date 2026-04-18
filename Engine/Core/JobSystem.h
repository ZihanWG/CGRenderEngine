// Small shared worker pool for background engine tasks such as async asset decode.
#pragma once

#include <cstddef>
#include <condition_variable>
#include <future>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class JobSystem
{
public:
    static JobSystem& Get();

    JobSystem();
    explicit JobSystem(std::size_t workerCount);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    std::size_t GetWorkerCount() const;

    template <typename Function>
    auto Submit(Function&& function) -> std::future<std::invoke_result_t<std::decay_t<Function>>>
    {
        using TaskType = std::decay_t<Function>;
        using ResultType = std::invoke_result_t<TaskType>;

        auto task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Function>(function));
        std::future<ResultType> future = task->get_future();
        Enqueue([task]() {
            (*task)();
        });
        return future;
    }

    template <typename Function>
    auto SubmitShared(Function&& function) -> std::shared_future<std::invoke_result_t<std::decay_t<Function>>>
    {
        return Submit(std::forward<Function>(function)).share();
    }

private:
    void Enqueue(std::function<void()> task);
    void StartWorkers(std::size_t workerCount);
    void WorkerLoop();

    mutable std::mutex m_Mutex;
    std::condition_variable m_Condition;
    std::queue<std::function<void()>> m_Tasks;
    std::vector<std::thread> m_Workers;
    bool m_Stopping = false;
};
