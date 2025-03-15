#pragma once

namespace MaVi
{
    constexpr double Seconds = 1.0;
    constexpr double Minutes = Seconds * 60.0;
    constexpr double Hours = Minutes * 60.0;
    constexpr double MilliSeconds = Seconds / 1000.0;
    constexpr double MicroSeconds = MilliSeconds / 1000.0;
    constexpr double NanoSeconds = MicroSeconds / 1000.0;
}

constexpr double operator""_Seconds(unsigned long long Val)
{
    return static_cast<double>(Val);
}

constexpr double operator""_Minutes(unsigned long long Val)
{
    return static_cast<double>(Val) * MaVi::Minutes;
}

constexpr double operator""_Hours(unsigned long long Val)
{
    return static_cast<double>(Val) * MaVi::Hours;
}

constexpr double operator""_MilliSeconds(unsigned long long Val)
{
    return static_cast<double>(Val) * MaVi::MilliSeconds;
}

constexpr double operator""_MicroSeconds(unsigned long long Val)
{
    return static_cast<double>(Val) * MaVi::MicroSeconds;
}

constexpr double operator""_NanoSeconds(unsigned long long Val)
{
    return static_cast<double>(Val) * MaVi::NanoSeconds;
}
