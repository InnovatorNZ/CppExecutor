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
    int max_size;
    std::deque<T> items;
    std::mutex queue_mutex;
    std::condition_variable not_empty_condition;
    std::condition_variable not_full_condition;

public:
    ArrayBlockingQueue(int size);

    bool offer(T e) override;

    bool offer(T e, long timeout) override;

    T poll() override;

    T poll(long timeout) override;

    void put(T e) override;

    T take() override;

    bool empty() const override;

    int remainingCapacity() const override;

    bool removeAt(int index);
};


#endif //CPPEXECUTOR_ARRAYBLOCKINGQUEUE_H
