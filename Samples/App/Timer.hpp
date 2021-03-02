#pragma once

namespace GoldenSun
{
    class Timer final
    {
    public:
        Timer();
        void Restart();

        // return elapsed time in seconds
        double Elapsed() const;

        // return estimated maximum value for Elapsed()
        double ElapsedMax() const;

        // return minimum value for Elapsed()
        double ElapsedMin() const;

        double CurrentTime() const;

    private:
        double start_time_;
    };
} // namespace GoldenSun
