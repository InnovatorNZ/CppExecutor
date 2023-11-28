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
#include <memory>
#include "BlockingQueue.h"
#if _WIN32
#include <Windows.h>
#endif

class RejectedExecutionException : public std::runtime_error {
public:
    explicit RejectedExecutionException(const std::string&);
};

class ThreadPoolExecutor;

class RejectedExecutionHandler {
public:
    RejectedExecutionHandler() = default;

    virtual void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) = 0;

    virtual ~RejectedExecutionHandler() = default;
};

class ThreadPoolExecutor {
    friend class ThreadPoolTest;

private:
    const int corePoolSize, maximumPoolSize;
    const long keepAliveTime;
    std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue;
    std::unique_ptr<RejectedExecutionHandler> rejectHandler;
    std::atomic<int> thread_cnt;
    int finished_cnt;
    std::condition_variable complete_condition;
    std::mutex finish_mutex;
    std::vector<std::thread> threads_;
    std::mutex thread_lock;
    volatile bool stop_;

public:
    class AbortPolicy : public RejectedExecutionHandler {
    public:
        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override;
    };

    class DiscardPolicy : public RejectedExecutionHandler {
    public:
        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override;
    };

    class DiscardOldestPolicy : public RejectedExecutionHandler {
    public:
        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override;
    };

    class CallerRunsPolicy : public RejectedExecutionHandler {
    public:
        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override;
    };

private:
    std::function<void()> createCoreThread(const std::function<void()>& firstTask);

    std::function<void()> createTempThread(const std::function<void()>& firstTask);

    void enqueue(const std::function<void()>& task);

    bool addWorker(bool core, const std::function<void()>& firstTask = nullptr);

    void reject(const std::function<void()>& task);

public:
    ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime,
                       std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue,
                       std::unique_ptr<RejectedExecutionHandler> rejectHandler);

    ~ThreadPoolExecutor();

    template<class F, class... Args>
    void execute(F&& f, Args&& ... args) {
        std::function<void()> cfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        enqueue(cfunc);
    }

    bool isShutdown() const;

    void waitForTaskComplete(int task_cnt);

    void resetFinishedCount() { finished_cnt = 0; }

#if _WIN32
    bool insidePool(DWORD);
#endif
};


#endif //CPPEXECUTOR_THREADPOOLEXECUTOR_H
