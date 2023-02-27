#include <iostream>
#include "ThreadPoolExecutor.h"
#include "ArrayBlockingQueue.h"
#include "sleep.h"

using namespace std;

int main() {

    {
        cout << "Single-thread test" << endl;
        ThreadPoolExecutor pool(2, 4, 3000,
                                new ArrayBlockingQueue<std::function<void()> >(2), ThreadPoolExecutor::DiscardPolicy);
        sleep(1);
        for (int i = 0; i < 9; i++) {
            cout << "Enqueue task " << i << endl;
            pool.execute([i] {
                cout << "Begin task " << i << endl;
                sleep(3);
                cout << "End task " << i << endl;
            });
            sleep(0.5);
        }
        sleep(8);
    }
    cout << endl;

    {
        cout << "Multi-thread test" << endl;
        ThreadPoolExecutor pool1(9, 9, 2500,
                                 new ArrayBlockingQueue<std::function<void()> >(0), ThreadPoolExecutor::DiscardPolicy);
        ThreadPoolExecutor pool2(2, 4, 2500,
                                 new ArrayBlockingQueue<std::function<void()> >(2), ThreadPoolExecutor::DiscardPolicy);
        for (int i = 0; i < 9; i++) {
            pool1.execute([i, &pool2] {
                for (int j = 0; j < 1; j++) {
                    cout << "Enqueue task (" << i << "," << j << ")" << endl;
                    pool2.execute([i, j] {
                        cout << "Begin task (" << i << "," << j << ")" << endl;
                        sleep(3);
                        cout << "End task (" << i << "," << j << ")" << endl;
                    });
                    sleep(0.5);
                }
            });
        }
        sleep(5);
    }

    return 0;
}