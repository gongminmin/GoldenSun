#include "pch.hpp"

#include "Timer.hpp"

#include <chrono>

namespace GoldenSun
{
    Timer::Timer()
    {
        this->Restart();
    }

    void Timer::Restart()
    {
        start_time_ = this->CurrentTime();
    }

    double Timer::Elapsed() const
    {
        return this->CurrentTime() - start_time_;
    }

    double Timer::ElapsedMax() const
    {
        return std::chrono::duration<double>::max().count();
    }

    double Timer::ElapsedMin() const
    {
        return std::chrono::duration<double>::min().count();
    }

    double Timer::CurrentTime() const
    {
        auto const tp = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(tp.time_since_epoch()).count();
    }
} // namespace GoldenSun
