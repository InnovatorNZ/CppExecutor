#include <iostream>
#include <random>
#include "ThreadPoolExecutor.h"
#include "ArrayBlockingQueue.h"
#include "ThreadPoolTest.h"

#if _WIN32

#include <Windows.h>

void __sleep(float sec) {
    Sleep(sec * 1000);
}

#else

#include <unistd.h>

void __sleep(float sec) {
    usleep(sec * 1000 * 1000);
}

#endif
using namespace std;

int main() {
    {
        cout << "Single-thread test" << endl;
        ThreadPoolExecutor pool(2, 4, 3000,
                                std::make_unique<ArrayBlockingQueue<std::function<void()> > >(2),
                                std::make_unique<ThreadPoolExecutor::DiscardOldestPolicy>());
        __sleep(1);
        for (int _ = 0; _ < 2; _++) {
            for (int i = 0; i < 9; i++) {
                cout << "Enqueue task " << i << endl;
                pool.execute([i] {
                    cout << "Begin task " << i << endl;
                    __sleep(4);
                    cout << "End task " << i << endl;
                });
                if (!ThreadPoolTest::check(pool)) cerr << "ERROR: BUG DETECTED!" << endl;
                __sleep(0.5);
            }
            __sleep(9);
        }
    }
    cout << endl;
    {
        cout << "Multi-thread test" << endl;
        ThreadPoolExecutor pool1(15, 15, 0,
                                 std::make_unique<ArrayBlockingQueue<std::function<void()> > >(0),
                                 std::make_unique<ThreadPoolExecutor::DiscardPolicy>());
        ThreadPoolExecutor pool2(32, 64, 200,
                                 std::make_unique<ArrayBlockingQueue<std::function<void()> > >(6),
                                 std::make_unique<ThreadPoolExecutor::DiscardPolicy>());
        for (int i = 0; i < 15; i++) {
            pool1.execute([i, &pool2] {
                std::mt19937 e(std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now()).time_since_epoch().count());
                normal_distribution<float> distribution(1, 0.2);
                for (int j = 0; j < 10; j++) {
                    cout << "Enqueue task (" << i << "," << j << ")" << endl;
                    float await_time = distribution(e);
                    pool2.execute([i, j, await_time] {
                        cout << "Begin task (" << i << "," << j << ")" << endl;
                        __sleep(await_time);
                        cout << "End task (" << i << "," << j << ")" << endl;
                    });
                    if (!ThreadPoolTest::check(pool2)) cerr << "ERROR: BUG DETECTED!" << endl;
                    __sleep(0.25);
                }
            });
        }
        __sleep(5);
    }
    cout << endl;

    {
        cout << "Wait until test" << endl;
        ThreadPoolExecutor pool(2, 4, 3000,
                                std::make_unique<ArrayBlockingQueue<std::function<void()> > >(2),
                                std::make_unique<ThreadPoolExecutor::DiscardOldestPolicy>());
        __sleep(1);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                cout << "Enqueue task " << j << endl;
                pool.execute([j] {
                    cout << "Begin task " << j << endl;
                    // __sleep(3);
                    cout << "End task " << j << endl;
                });
                if (!ThreadPoolTest::check(pool)) cerr << "ERROR: BUG DETECTED!" << endl;
                // __sleep(0.2);
            }
            pool.waitForTaskComplete(6);
        }
    }
    return 0;
}