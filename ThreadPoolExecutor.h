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

class RejectedExecutionException;

class RejectedExecutionHandler;

class ThreadPoolExecutor {
    friend class Test;

private:
    const int corePoolSize, maximumPoolSize;
    const long keepAliveTime;
    RejectedExecutionHandler* rejectHandler;
    std::atomic<int> thread_cnt;
    int working_cnt;
    std::condition_variable complete_condition;
    std::mutex finish_mutex;
    std::vector<std::thread> threads_;
    std::mutex thread_lock;
    std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue;
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
    std::function<void()> createCoreThread(const std::function<void()>& firstTask);

    std::function<void()> createTempThread(const std::function<void()>& firstTask);

    void enqueue(const std::function<void()>& task);

    bool addWorker(bool core, const std::function<void()>& firstTask = nullptr);

    void reject(const std::function<void()>& task);

public:
    ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime,
                       std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue, RejectedExecutionHandler* rejectHandler);

    ~ThreadPoolExecutor();

    template<class F, class... Args>
    void execute(F&& f, Args&& ... args) {
        std::function<void()> cfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        enqueue(cfunc);
    }

    bool isShutdown() const;

    void waitForTaskComplete();

#if _WIN32
    bool insidePool(DWORD);
#endif
};


#endif //CPPEXECUTOR_THREADPOOLEXECUTOR_H
