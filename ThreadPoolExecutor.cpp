#include "ThreadPoolExecutor.h"

RejectedExecutionException::RejectedExecutionException(const std::string& arg) : runtime_error(arg) {
    std::cerr << arg << std::endl;
}

void ThreadPoolExecutor::AbortPolicy::rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) {
    throw RejectedExecutionException("Task rejected!");
}

void ThreadPoolExecutor::DiscardPolicy::rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) {
    std::cerr << "Task rejected!" << std::endl;
}

void ThreadPoolExecutor::DiscardOldestPolicy::rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) {
    if (!e->isShutdown()) {
        e->workQueue->poll();
        e->workQueue->put(func);
    }
}

void ThreadPoolExecutor::CallerRunsPolicy::rejectedExecution(const std::function<void()>& func, ThreadPoolExecutor* e) {
    if (!e->isShutdown()) {
        func();
    }
}

ThreadPoolExecutor::ThreadPoolExecutor(int corePoolSize, int maximumPoolSize, long keepAliveTime,
                                       std::unique_ptr<BlockingQueue<std::function<void()> > > workQueue, 
                                       std::unique_ptr<RejectedExecutionHandler> rejectHandler) :
        corePoolSize(corePoolSize), maximumPoolSize(maximumPoolSize), keepAliveTime(keepAliveTime),
        workQueue(std::move(workQueue)), rejectHandler(std::move(rejectHandler)), thread_cnt(0), finished_cnt(0), stop_(false) {
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
            firstTask();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                finished_cnt++;
                complete_condition.notify_all();
            }
        }
        while (true) {
            auto task = workQueue->take();
            if (stop_ || task == nullptr) {
                thread_cnt--;
                return;
            }
            task();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                finished_cnt++;
                complete_condition.notify_all();
            }
        }
    };
}

std::function<void()> ThreadPoolExecutor::createTempThread(const std::function<void()>& firstTask) {
    return [this, firstTask] {
        if (firstTask != nullptr) {
            firstTask();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                finished_cnt++;
                complete_condition.notify_all();
            }
        }
        while (true) {
            auto task = workQueue->poll(keepAliveTime);
            if (stop_ || task == nullptr) {
                thread_cnt--;
                std::clog << "Temp thread exited." << std::endl;
                return;
            }
            task();
            {
                std::unique_lock<std::mutex> lock(finish_mutex);
                finished_cnt++;
                complete_condition.notify_all();
            }
        }
    };
}

void ThreadPoolExecutor::waitForTaskComplete(int task_cnt) {
    std::this_thread::yield();
    auto task_finished = [this, task_cnt] { return finished_cnt == task_cnt && workQueue->empty(); };
    while (true) {
        std::unique_lock<std::mutex> lock(finish_mutex);
        this->complete_condition.wait(lock, task_finished);
        if (task_finished()) {
            finished_cnt = 0;
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
    {
        std::unique_lock<std::mutex> lock(finish_mutex);
        finished_cnt++;
        complete_condition.notify_all();
    }
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