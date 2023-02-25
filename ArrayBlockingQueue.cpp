#include "ArrayBlockingQueue.h"

template<typename T>
ArrayBlockingQueue<T>::ArrayBlockingQueue(int size) : max_size(size) {
}

template<typename T>
bool ArrayBlockingQueue<T>::offer(T e) {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.size() >= max_size) return false;
        items.push_back(e);
    }
    not_empty_condition.notify_one();
    return true;
}

template<typename T>
bool ArrayBlockingQueue<T>::offer(T e, long timeout) {
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

template<typename T>
T ArrayBlockingQueue<T>::poll() {
    if (items.empty()) return nullptr;
    this->queue_mutex.lock();
    T ret = std::move(items.front());
    items.pop_front();
    this->queue_mutex.unlock();
    not_full_condition.notify_one();
    return ret;
}

template<typename T>
T ArrayBlockingQueue<T>::poll(long timeout) {
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    if (items.empty()) {
        std::cv_status status = not_empty_condition.wait_for(lock, std::chrono::milliseconds(timeout));
        if (status == std::cv_status::timeout) {
            return nullptr;
        }
    }
    T ret = std::move(items.front());
    items.pop_front();
    lock.unlock();
    not_full_condition.notify_one();
    return ret;
}

template<typename T>
void ArrayBlockingQueue<T>::put(T e) {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.size() >= max_size) {
            not_full_condition.wait(lock);
        }
        items.push_back(e);
    }
    not_empty_condition.notify_one();
}

template<typename T>
T ArrayBlockingQueue<T>::take() {
    std::unique_lock<std::mutex> lock(this->queue_mutex);
    if (items.empty()) {
        not_empty_condition.wait(lock);
    }
    T ret = std::move(items.front());
    items.pop_front();
    lock.unlock();
    not_full_condition.notify_one();
    return ret;
}

template<typename T>
bool ArrayBlockingQueue<T>::empty() const {
    return this->items.empty();
}

template<typename T>
int ArrayBlockingQueue<T>::remainingCapacity() const {
    return max_size - items.size();
}

template<typename T>
bool ArrayBlockingQueue<T>::removeAt(int index) {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (items.size() <= index) return false;
        items.erase(index);
    }
    not_full_condition.notify_one();
    return true;
}