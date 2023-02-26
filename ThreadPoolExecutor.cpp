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
    }
};

class ThreadPoolExecutor::DiscardOldestPolicy_ : public RejectedExecutionHandler {
public:
    DiscardOldestPolicy_() = default;

    void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        if (!e->isShutdown()) {
            e->task_queue_mutex.lock();
            e->taskQueue.pop_back();
            e->taskQueue.push_back(func);
            e->task_queue_mutex.unlock();
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

ThreadPoolExecutor::ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, int queueSize,
                                       RejectedExecutionHandler* rejectHandler) :
        corePoolSize(corePoolSize), maximumPoolSize(maximumPoolSize), keepAliveTime(keepAliveTime),
        queueSize(queueSize), rejectHandler(rejectHandler), stop_(false), thread_cnt(0) {
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
    {
        std::unique_lock<std::mutex> lock(task_queue_mutex);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker: threads_)
        worker.join();
}

std::function<void()> ThreadPoolExecutor::createRegularThread(const std::function<void()>& firstTask) {
    return [this, firstTask] {
        firstTask();
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(this->task_queue_mutex);
                this->condition_.wait(lock,
                                      [this] { return this->stop_ || !this->taskQueue.empty(); });
                if (this->stop_ && this->taskQueue.empty())
                    return;
                task = std::move(this->taskQueue.front());
                this->taskQueue.pop_front();
            }
            task();
        }
    };
}

std::function<void()> ThreadPoolExecutor::createTempThread(const std::function<void()>& firstTask) {
    return [this, firstTask] {
        firstTask();
        while (true) {
            std::function<void()> task;
            auto const timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(keepAliveTime);
            {
                std::unique_lock<std::mutex> lock(this->task_queue_mutex);
                this->condition_.wait_for(lock, std::chrono::milliseconds(keepAliveTime),
                                          [this] { return this->stop_ || !this->taskQueue.empty(); });
                if (std::chrono::steady_clock::now() > timeout) {
                    std::clog << "Timeout, temporary thread exited." << std::endl;
                    return;
                } else if (this->stop_ && this->taskQueue.empty())
                    return;
                task = std::move(this->taskQueue.front());
                this->taskQueue.pop_front();
            }
            task();
        }
    };
}

void ThreadPoolExecutor::enqueue(const std::function<void()>& task) {
    //thread_queue_mutex.lock();
    int c_thread_num;
    while (true) {
        c_thread_num = thread_cnt;
        if (c_thread_num < corePoolSize) {
            if (thread_cnt.compare_exchange_weak(c_thread_num, c_thread_num + 1))
                break;
        }
    }
    if (c_thread_num < corePoolSize) {
        threads_.emplace_back(createRegularThread(task));
    } else if (taskQueue.size() >= queueSize) {
        if (c_thread_num >= maximumPoolSize) {
            task_queue_mutex.unlock();
            std::cerr << "Reject policy triggered!" << std::endl;
            this->rejectHandler->rejectedExecution(task, this);
            return;
        } else {
            task_queue_mutex.unlock();
            std::clog << "Adding temporary thread." << std::endl;
            std::thread temp_thread = std::thread(createTempThread(task));
            {
                std::unique_lock lock(this->thread_queue_mutex);
                threads_.push_back(std::move(temp_thread));
            }
        }
    } else {
        taskQueue.emplace_back(task);
        task_queue_mutex.unlock();
        condition_.notify_one();
    }
}

bool ThreadPoolExecutor::isShutdown() const {
    return this->stop_;
}