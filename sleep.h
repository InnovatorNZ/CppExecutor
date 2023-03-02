#ifndef CPPEXECUTOR_SLEEP_H
#define CPPEXECUTOR_SLEEP_H

#if _WIN32

#include <windows.h>

void __sleep(float sec) {
    Sleep(sec * 1000);
}

#else

#include <unistd.h>

void __sleep(float sec) {
    usleep(sec * 1000 * 1000);
}

#endif
#endif //CPPEXECUTOR_SLEEP_H