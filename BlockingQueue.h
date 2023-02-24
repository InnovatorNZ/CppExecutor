#ifndef CPPEXECUTOR_BLOCKINGQUEUE_H
#define CPPEXECUTOR_BLOCKINGQUEUE_H

template<typename T>
class BlockingQueue {
public:
    virtual bool offer(T e) = 0;

    virtual bool offer(T e, long timeout) = 0;

    virtual T poll() = 0;

    virtual T poll(long timeout) = 0;

    virtual void put(T e) = 0;

    virtual T take() = 0;

    virtual bool empty() const = 0;

    virtual int remainingCapacity() const = 0;
};


#endif //CPPEXECUTOR_BLOCKINGQUEUE_H
