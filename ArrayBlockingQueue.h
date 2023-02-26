#ifndef CPPEXECUTOR_ARRAYBLOCKINGQUEUE_H
#define CPPEXECUTOR_ARRAYBLOCKINGQUEUE_H

#include "BlockingQueue.h"
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<typename T>
class ArrayBlockingQueue : public BlockingQueue<T> {
private:
    size_t max_size;
    std::deque<T> items;
    std::mutex queue_mutex;
    std::condition_variable not_empty_condition;
    std::condition_variable not_full_condition;
    bool stop_;

public:
    ArrayBlockingQueue() = delete;

    explicit ArrayBlockingQueue(size_t size) : max_size(size), stop_(false) {
    }

    bool offer(T e) override {
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            if (items.size() >= max_size) return false;
            items.push_back(e);
        }
        not_empty_condition.notify_one();
        return true;
    }

    bool offer(T e, long timeout) override {
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            if (items.size() >= max_size) {
                std::cv_status status = not_full_condition.wait_for(lock, std::chrono::milliseconds(timeout));
                if (status == std::cv_status::timeout)
                    return false;
            }
            items.push_back(e);
        }
        not_empty_condition.notify_one();
        return true;
    }

    T poll() override {
        if (items.empty()) return nullptr;
        this->queue_mutex.lock();
        T ret = std::move(items.front());
        items.pop_front();
        this->queue_mutex.unlock();
        not_full_condition.notify_one();
        return ret;
    }

    T poll(long timeout) override {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.empty()) {
            bool status = not_empty_condition.wait_for(lock, std::chrono::milliseconds(timeout),
                                                       [this] { return stop_ || !items.empty(); });
            if (!status || stop_ && items.empty()) {
                return nullptr;
            }
        }
        T ret = std::move(items.front());
        items.pop_front();
        lock.unlock();
        not_full_condition.notify_one();
        return ret;
    }

    T peek() override {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.empty()) return nullptr;
        return items.front();
    }

    void put(T e) override {
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            if (items.size() >= max_size) {
                not_full_condition.wait(lock);
            }
            items.push_back(e);
        }
        not_empty_condition.notify_one();
    }

    T take() override {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.empty()) {
            not_empty_condition.wait(lock, [this] { return stop_ || !items.empty(); });
            if (stop_ && items.empty()) {
                return nullptr;
            }
        }
        T ret = std::move(items.front());
        items.pop_front();
        lock.unlock();
        not_full_condition.notify_one();
        return ret;
    }

    bool empty() const override {
        return this->items.empty();
    }

    int remainingCapacity() const override {
        return max_size - items.size();
    }

    bool removeAt(int index) {
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            if (items.size() <= index) return false;
            items.erase(index);
        }
        not_full_condition.notify_one();
        return true;
    }

    void close() override {
        this->queue_mutex.lock();
        stop_ = true;
        this->queue_mutex.unlock();
        not_empty_condition.notify_all();
        not_full_condition.notify_all();
    }
};


#endif //CPPEXECUTOR_ARRAYBLOCKINGQUEUE_H
