#pragma once

#include <compare>

struct Singular
{
    int mOverflow = 0;
    int mNaN = 0;
    constexpr explicit Singular() = default;
    constexpr explicit Singular(bool is_overflow, bool is_nan) : mOverflow{is_overflow}, mNaN{is_nan} {};
    constexpr Singular(const Singular &other) = default;
    constexpr Singular(Singular &&other) = default;
    constexpr Singular &operator=(const Singular &other) = default;
    constexpr Singular &operator=(Singular &&other) = default;
    bool operator()() const { return mNaN != 0 || mOverflow != 0; }
    bool operator==(const Singular &other) const
    {
        return !(mOverflow || other.mOverflow || mNaN || other.mNaN);
    }
    auto operator<=>(const Singular &other) const = default;
    bool IsOverflow() const { return mOverflow != 0; }
    bool IsNaN() const { return mNaN != 0; }
};
