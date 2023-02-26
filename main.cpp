#include <iostream>
#include "ThreadPoolExecutor.h"
#include "sleep.h"

int main() {
    ThreadPoolExecutor pool(2, 4, 4000, 2, ThreadPoolExecutor::DiscardPolicy);
    sleep(1);
    for (int i = 0; i < 9; i++) {
        std::cout << "Enqueue task " << i << std::endl;
        pool.execute([i] {
            std::cout << "Begin task " << i << std::endl;
            sleep(3);
            std::cout << "End task " << i << std::endl;
        });
        sleep(0.5);
    }
    sleep(10);
    return 0;
}