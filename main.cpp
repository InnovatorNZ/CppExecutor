#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

#if _WIN32

#include <windows.h>

void sleep(float sec) {
    Sleep(sec * 1000);
}

#else
#include <unistd.h>
void sleep(float sec) {
    usleep(sec * 1000 * 1000);
}
#endif

class ThreadPoolExecutor;

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

class ThreadPoolExecutor {
private:
    const int corePoolSize, maximumPoolSize, queueSize;
    const long keepAliveTime;
    RejectedExecutionHandler* rejectHandler;
    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> taskQueue;
    std::mutex task_queue_mutex, thread_queue_mutex;
    std::condition_variable condition_;
    bool stop_;

    class AbortPolicy_ : public RejectedExecutionHandler {
    public:
        AbortPolicy_() = default;

        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
            throw RejectedExecutionException("Task rejected!");
        }
    };

    class DiscardPolicy_ : public RejectedExecutionHandler {
    public:
        DiscardPolicy_() = default;

        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
        }
    };

    class DiscardOldestPolicy_ : public RejectedExecutionHandler {
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

    class CallerRunsPolicy_ : public RejectedExecutionHandler {
    public:
        CallerRunsPolicy_() = default;

        void rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) override {
            if (!e->isShutdown()) {
                func();
            }
        }
    };

public:
    static RejectedExecutionHandler* AbortPolicy;
    static RejectedExecutionHandler* DiscardPolicy;
    static RejectedExecutionHandler* CallerRunsPolicy;
    static RejectedExecutionHandler* DiscardOldestPolicy;

private:
    std::function<void()> createRegularThread(const std::function<void()>& firstTask = {}) {
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

    std::function<void()> createTempThread(const std::function<void()>& firstTask = {}) {
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

public:
    ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, int queueSize, RejectedExecutionHandler* rejectHandler) :
            corePoolSize(corePoolSize), maximumPoolSize(maximumPoolSize), keepAliveTime(keepAliveTime),
            queueSize(queueSize), rejectHandler(rejectHandler), stop_(false) {
    }

    ~ThreadPoolExecutor() {
        {
            std::unique_lock<std::mutex> lock(task_queue_mutex);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker: threads_)
            worker.join();
    }

    template<class F, class... Args>
    void enqueue(F&& f, Args&& ... args) {
        std::function<void()> cfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        task_queue_mutex.lock();
        int c_thread_num = this->threads_.size();
        if (c_thread_num < corePoolSize) {
            task_queue_mutex.unlock();
            std::thread regular_thread = std::thread(createRegularThread(cfunc));
            {
                std::unique_lock lock(this->thread_queue_mutex);
                threads_.push_back(std::move(regular_thread));
            }
        } else if (taskQueue.size() >= queueSize) {
            if (c_thread_num >= maximumPoolSize) {
                task_queue_mutex.unlock();
                std::cerr << "Reject policy triggered!" << std::endl;
                this->rejectHandler->rejectedExecution(cfunc, this);
                return;
            } else {
                task_queue_mutex.unlock();
                std::clog << "Adding temporary thread." << std::endl;
                std::thread temp_thread = std::thread(createTempThread(cfunc));
                {
                    std::unique_lock lock(this->thread_queue_mutex);
                    threads_.push_back(std::move(temp_thread));
                }
            }
        } else {
            taskQueue.emplace_back(cfunc);
            task_queue_mutex.unlock();
            condition_.notify_one();
        }
    }

    bool isShutdown() const {
        return this->stop_;
    }
};

RejectedExecutionHandler* ThreadPoolExecutor::AbortPolicy = new ThreadPoolExecutor::AbortPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::DiscardPolicy = new ThreadPoolExecutor::DiscardPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::CallerRunsPolicy = new ThreadPoolExecutor::CallerRunsPolicy_();
RejectedExecutionHandler* ThreadPoolExecutor::DiscardOldestPolicy = new ThreadPoolExecutor::DiscardOldestPolicy_();

int main() {
    ThreadPoolExecutor pool(2, 4, 4000, 2, ThreadPoolExecutor::DiscardPolicy);
    sleep(1);
    for (int i = 0; i < 9; i++) {
        std::cout << "Enqueue task " << i << std::endl;
        pool.enqueue([i] {
            std::cout << "Begin task " << i << std::endl;
            sleep(3);
            std::cout << "End task " << i << std::endl;
        });
        sleep(0.5);
    }
    sleep(10);
    return 0;
}