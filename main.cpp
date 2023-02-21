#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


using namespace std;

class ThreadPool {
public:
    ThreadPool(int thread_num) : stop_(false) {
        for (int i = 0; i < thread_num; ++i) {
            threads_.emplace_back(
                    [this] {
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
                    }
            );
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread &worker: threads_)
            worker.join();
    }

    template<class F, class... Args>
    void enqueue(F &&f, Args &&... args) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        }
        condition_.notify_one();
    }

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

int main() {
    ThreadPool pool(4);
    for (int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            cout << "Hello " << i << endl;
#if _WIN32
            Sleep(i * 1000);
#else
            usleep(i * 1000 * 1000);
#endif
            cout << "Bye " << i << endl;
        });
    }
    return 0;
}