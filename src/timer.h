#ifndef GESTUREFACE_TIMER_H
#define GESTUREFACE_TIMER_H

#include <chrono>

class HighClock {
public:
    HighClock() {}

    void Start() {
        start = std::chrono::high_resolution_clock::now();
    }

    void Stop() {
        stop = std::chrono::high_resolution_clock::now();
    }

    double GetTime() {
        return std::chrono::duration<double, std::micro>(stop - start).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point stop;
};

#endif //GESTUREFACE_TIMER_H
