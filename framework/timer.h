#pragma once
#include <chrono>
#include <cassert>

class Timer
{
public:
    void run()
    {
        prev = std::chrono::high_resolution_clock::now();
        running = true;
    }

    float millisecondsElapsed()
    {
        assert(running);
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::microseconds ms =
            std::chrono::duration_cast<std::chrono::microseconds>(now - prev);
        prev = now;
        return static_cast<float>(ms.count()) * .001f;
    }

    float secondsElapsed()
    {
        return millisecondsElapsed() * .001f;
    }

private:
    std::chrono::high_resolution_clock::time_point prev;
    bool running = false;
};
