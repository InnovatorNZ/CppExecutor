#ifndef CPPEXECUTOR_TEST_H
#define CPPEXECUTOR_TEST_H

#include "ThreadPoolExecutor.h"

class Test {
public:
    static bool check(const ThreadPoolExecutor& e) {
        return e.thread_cnt <= e.maximumPoolSize &&
               e.workQueue->remainingCapacity() >= 0;
    }
};

#endif //CPPEXECUTOR_TEST_H