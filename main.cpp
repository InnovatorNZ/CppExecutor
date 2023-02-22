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


class ThreadPoolExecutor {
private:
    const int corePoolSize, maximumPoolSize, queueSize;
    const long keepAliveTime;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;

private:
    std::function<void()> createRegularThread(const std::function<void()>& firstTask = {}) {
        return [this, firstTask] {
            firstTask();
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    this->condition_.wait(lock,
                                          [this] { return this->stop_ || !this->tasks_.empty(); });
                    if (this->stop_ && this->tasks_.empty())
                        return;
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
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
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    this->condition_.wait_for(lock, std::chrono::milliseconds(keepAliveTime),
                                              [this] { return this->stop_ || !this->tasks_.empty(); });
                    if (std::chrono::steady_clock::now() > timeout) {
                        std::clog << "Timeout, temporary thread exited." << std::endl;
                        return;
                    } else if (this->stop_ && this->tasks_.empty())
                        return;
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                task();
            }
        };
    }

public:
    ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime, int queueSize) :
            corePoolSize(corePoolSize), maximumPoolSize(maximumPoolSize), keepAliveTime(keepAliveTime),
            queueSize(queueSize), stop_(false) {
    }

    ~ThreadPoolExecutor() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker: threads_)
            worker.join();
    }

    template<class F, class... Args>
    void enqueue(F&& f, Args&& ... args) {
        {
            std::function<void()> cfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            std::unique_lock<std::mutex> lock(queue_mutex_);
            int c_thread_num = this->threads_.size();
            if (c_thread_num < corePoolSize) {
                threads_.emplace_back(createRegularThread(cfunc));
            } else if (tasks_.size() >= queueSize) {
                if (c_thread_num >= maximumPoolSize) {
                    // Reject policy
                    std::cerr << "Rejected!" << std::endl;
                    return;
                } else {
                    std::clog << "Adding temporary thread." << std::endl;
                    threads_.emplace_back(createTempThread(cfunc));
                }
            } else {
                tasks_.emplace(cfunc);
            }
        }
        condition_.notify_one();
    }
};

int main() {
    ThreadPoolExecutor pool(2, 4, 4000, 2);
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