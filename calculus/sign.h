#pragma once

#include <compare>

struct Sign
{
    int mSign = 0;
    constexpr explicit Sign() = default;
    constexpr Sign(const Sign &other) = default;
    constexpr Sign(Sign &&other) = default;
    constexpr Sign(bool value) : mSign{value} {};
    constexpr Sign &operator=(const Sign &other) = default;
    constexpr Sign &operator=(Sign &&other) = default;
    Sign &operator^(const Sign &other)
    {
        this->mSign = operator()() ^ other.operator()();
        return *this;
    }
    bool operator()() const { return mSign != 0; }
    void operator-()
    {
        mSign = 1 - operator()();
    }
    bool operator==(const Sign &other) const
    {
        return mSign == other.mSign ? true : ((operator()() && other.operator()()) || (!operator()() && !other.operator()()));
    }
    auto operator<=>(const Sign &other) = delete;
};
