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
            if (status == std::cv_status::no_timeout)
                items.push_back(e);
            else
                return false;
        } else {
            items.push(e);
        }
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
        if (status == std::cv_status::no_timeout) {
            T ret = std::move(items.front());
            items.pop_front();
        } else
            return nullptr;
    }
}

template<typename T>
void ArrayBlockingQueue<T>::put(T e) {

}

template<typename T>
T ArrayBlockingQueue<T>::take() {
    return nullptr;
}

template<typename T>
bool ArrayBlockingQueue<T>::empty() const {
    return this->items.empty();
}

template<typename T>
int ArrayBlockingQueue<T>::remainingCapacity() const {
    return 0;
}
