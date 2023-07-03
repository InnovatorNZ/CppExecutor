#include "ThreadPoolExecutor.h"

class RejectedExecutionException : public std::runtime_error {
public:
    RejectedExecutionException(const std::string& arg) : runtime_error(arg) {
        std::cerr << arg << std::endl;
    }
};

class RejectedExecutionHandler {
public:
    virtual void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) = 0;
};

class ThreadPoolExecutor::AbortPolicy_ : public RejectedExecutionHandler {
public:
    AbortPolicy_() = default;

    void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        throw RejectedExecutionException("Task rejected!");
    }
};

class ThreadPoolExecutor::DiscardPolicy_ : public RejectedExecutionHandler {
public:
    DiscardPolicy_() = default;

    void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        std::cerr << "Task rejected!" << std::endl;
    }
};

class ThreadPoolExecutor::DiscardOldestPolicy_ : public RejectedExecutionHandler {
public:
    DiscardOldestPolicy_() = default;

    void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        if (!e->isShutdown()) {
            e->workQueue->poll();
            e->workQueue->put(func);
        }
    }
};

class ThreadPoolExecutor::CallerRunsPolicy_ : public RejectedExecutionHandler {
public:
    CallerRunsPolicy_() = default;

    void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        if (!e->isShutdown()) {
            func();
        }
    }
};

RejectedExecutionHandler* ThreadPoolExecutor::AbortPolicy = new ThreadPoolExecutor::AbortPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::DiscardPolicy = new ThreadPoolExecutor::DiscardPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::CallerRunsPolicy = new ThreadPoolExecutor::CallerRunsPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::DiscardOldestPolicy = new ThreadPoolExecutor::DiscardOldestPolicy_();

ThreadPoolExecutor::ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime,
                                       std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue, RejectedExecutionHandler* rejectHandler) :
        corePoolSize(corePoolSize), maximumPoolSize(maximumPoolSize), keepAliveTime(keepAliveTime),
        workQueue(std::move(workQueue)), rejectHandler(rejectHandler), thread_cnt(0), working_cnt(0), stop_(false) {
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
    stop_ = true;
    workQueue->close();
    for (std::thread& worker : threads_)
        worker.join();
}

std::function<void()> ThreadPoolExecutor::createCoreThread(const std::function<void()>& firstTask) {
    return [this, firstTask] {
        if (firstTask != nullptr) {
            std::unique_lock<std::mutex> finish_lock(finish_mutex);
            working_cnt++;
            finish_lock.unlock();
            firstTask();
            finish_lock.lock();
            working_cnt--;
            complete_condition.notify_all();
        }
        while (true) {
            auto task = workQueue->take();
            if (stop_ || task == nullptr) {
                thread_cnt--;
                return;
            }
            std::unique_lock<std::mutex> finish_lock(finish_mutex);
            working_cnt++;
            finish_lock.unlock();
            task();
            finish_lock.lock();
            working_cnt--;
            complete_condition.notify_all();
        }
    };
}

std::function<void()> ThreadPoolExecutor::createTempThread(const std::function<void()>& firstTask) {
    return [this, firstTask] {
        if (firstTask != nullptr) {
            working_cnt++;
            firstTask();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                working_cnt--;
            }
            complete_condition.notify_all();
        }
        while (true) {
            auto task = workQueue->poll(keepAliveTime);
            if (stop_ || task == nullptr) {
                thread_cnt--;
                std::clog << "Temp thread exited." << std::endl;
                return;
            }
            working_cnt++;
            task();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                working_cnt--;
            }
            complete_condition.notify_all();
        }
    };
}

void ThreadPoolExecutor::waitForTaskComplete() {
    std::this_thread::yield();
    while (true) {
        std::unique_lock<std::mutex> lock(finish_mutex);
        this->complete_condition.wait(lock, [this] { return working_cnt == 0 && workQueue->empty(); });
        if (working_cnt == 0 && workQueue->empty()) {
            return;
        }
    }
}

bool ThreadPoolExecutor::addWorker(bool core, const std::function<void()>& firstTask) {
    while (true) {
        int c_thread_num = this->thread_cnt;
        if (core) {
            if (c_thread_num < corePoolSize) {
                if (thread_cnt.compare_exchange_weak(c_thread_num, c_thread_num + 1))
                    break;
            } else {
                return false;
            }
        } else {
            if (c_thread_num < maximumPoolSize) {
                if (thread_cnt.compare_exchange_weak(c_thread_num, c_thread_num + 1))
                    break;
            } else {
                return false;
            }
        }
    }
    if (core) {
        std::thread th(createCoreThread(firstTask));
        std::unique_lock lock(this->thread_lock);
        this->threads_.emplace_back(std::move(th));
    } else {
        std::thread th(createTempThread(firstTask));
        std::unique_lock lock(this->thread_lock);
        this->threads_.emplace_back(std::move(th));
    }
    return true;
}

void ThreadPoolExecutor::enqueue(const std::function<void()>& task) {
    if (thread_cnt < corePoolSize && addWorker(true, task)) {
        return;
    } else if (workQueue->offer(task)) {
        if (thread_cnt == 0)
            addWorker(false);
    } else if (!addWorker(false, task)) {
        reject(task);
    }
}

void ThreadPoolExecutor::reject(const std::function<void()>& task) {
    rejectHandler->rejectedExecution(task, this);
}

bool ThreadPoolExecutor::isShutdown() const {
    return this->stop_;
}

#if _WIN32
bool ThreadPoolExecutor::insidePool(DWORD thID) {
    for (int i = 0; i < threads_.size(); i++) {
        if (threads_[i].joinable()) {
            DWORD cthId = ::GetThreadId(static_cast<HANDLE>(threads_[i].native_handle()));
            if (cthId == thID) return true;
        }
    }
    return false;
}
#endif