#ifndef CPPEXECUTOR_BLOCKINGQUEUE_H
#define CPPEXECUTOR_BLOCKINGQUEUE_H

template<typename T>
class BlockingQueue {
public:
    virtual bool push_back(T e) = 0;

    virtual bool push_front(T e) = 0;

    virtual T pop_back() = 0;

    virtual T pop_front() = 0;

    virtual T pop_back(long timeout) = 0;

    virtual T pop_front(long timeout) = 0;

    virtual bool empty() const = 0;
};


#endif //CPPEXECUTOR_BLOCKINGQUEUE_H
