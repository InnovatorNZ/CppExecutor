#ifndef CPPEXECUTOR_THREADPOOLTEST_H
#define CPPEXECUTOR_THREADPOOLTEST_H

#include "ThreadPoolExecutor.h"

class ThreadPoolTest {
public:
    static bool check(const ThreadPoolExecutor& e) {
        return e.thread_cnt <= e.maximumPoolSize &&
               e.workQueue->remainingCapacity() >= 0;
    }
};

#endif //CPPEXECUTOR_THREADPOOLTEST_H