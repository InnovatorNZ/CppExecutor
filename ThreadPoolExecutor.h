#ifndef CPPEXECUTOR_THREADPOOLEXECUTOR_H
#define CPPEXECUTOR_THREADPOOLEXECUTOR_H

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class RejectedExecutionException;

class RejectedExecutionHandler;

class ThreadPoolExecutor {
private:
    const int corePoolSize, maximumPoolSize, queueSize;
    const long keepAliveTime;
    RejectedExecutionHandler* rejectHandler;
    std::atomic_int thread_cnt;
    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> taskQueue;
    std::mutex task_queue_mutex, thread_queue_mutex;
    std::condition_variable condition_;
    bool stop_;

private:
    class AbortPolicy_;

    class DiscardPolicy_;

    class DiscardOldestPolicy_;

    class CallerRunsPolicy_;

public:
    static RejectedExecutionHandler* AbortPolicy;
    static RejectedExecutionHandler* DiscardPolicy;
    static RejectedExecutionHandler* CallerRunsPolicy;
    static RejectedExecutionHandler* DiscardOldestPolicy;

private:
    std::function<void()> createRegularThread(const std::function<void()>& firstTask = {});

    std::function<void()> createTempThread(const std::function<void()>& firstTask = {});

    void enqueue(const std::function<void()>& task);

public:
    ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, int queueSize, RejectedExecutionHandler* rejectHandler);

    ~ThreadPoolExecutor();

    template<class F, class... Args>
    void execute(F&& f, Args&& ... args) {
        std::function<void()> cfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        enqueue(cfunc);
    }

    bool isShutdown() const;
};


#endif //CPPEXECUTOR_THREADPOOLEXECUTOR_H
