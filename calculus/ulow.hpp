#pragma once

#include <cstdint>
#include <bit>
#include <utility>   // std::pair, std::tie
#include <limits>    // std::numeric_limits

namespace low64
{
/**
     * @brief Класс для хранения 64-битного беззнакового числа ("половинки").
     */
class ULOW
{
public:
    // --- Конструкторы ---
    constexpr ULOW() noexcept = default;
    constexpr ULOW(uint64_t value) noexcept : mValue(value) {}

    // --- Сравнение (C++20) ---
    auto operator<=>(const ULOW&) const = default;

    // --- Доступ к данным ---
    [[nodiscard]] constexpr uint64_t operator()() const noexcept { return mValue; }
    [[nodiscard]] constexpr uint64_t& operator()() noexcept { return mValue; }

    // --- Унарные операторы ---
    [[nodiscard]] constexpr ULOW operator~() const noexcept { return ULOW{~mValue}; }
    [[nodiscard]] constexpr ULOW operator-() const noexcept { return ULOW{0ull - mValue}; }

    // --- Бинарная арифметика и битовые операции ---
    // Используем лаконичную форму для лучшей оптимизации NRVO
    [[nodiscard]] constexpr ULOW operator<<(uint32_t s) const noexcept { return ULOW{mValue << s}; }
    [[nodiscard]] constexpr ULOW operator>>(uint32_t s) const noexcept { return ULOW{mValue >> s}; }

    constexpr ULOW& operator<<=(uint32_t s) noexcept { mValue <<= s; return *this; }
    constexpr ULOW& operator>>=(uint32_t s) noexcept { mValue >>= s; return *this; }

    [[nodiscard]] constexpr ULOW operator&(const ULOW& rhs) const noexcept { return ULOW{mValue & rhs.mValue}; }
    [[nodiscard]] constexpr ULOW operator|(const ULOW& rhs) const noexcept { return ULOW{mValue | rhs.mValue}; }
    [[nodiscard]] constexpr ULOW operator^(const ULOW& rhs) const noexcept { return ULOW{mValue ^ rhs.mValue}; }

    constexpr ULOW& operator&=(const ULOW& rhs) noexcept { mValue &= rhs.mValue; return *this; }
    constexpr ULOW& operator|=(const ULOW& rhs) noexcept { mValue |= rhs.mValue; return *this; }
    constexpr ULOW& operator^=(const ULOW& rhs) noexcept { mValue ^= rhs.mValue; return *this; }

    [[nodiscard]] constexpr ULOW operator+(const ULOW& rhs) const noexcept { return ULOW{mValue + rhs.mValue}; }
    [[nodiscard]] constexpr ULOW operator-(const ULOW& rhs) const noexcept { return ULOW{mValue - rhs.mValue}; }
    [[nodiscard]] constexpr ULOW operator*(const ULOW& rhs) const noexcept { return ULOW{mValue * rhs.mValue}; }

    constexpr ULOW& operator+=(const ULOW& rhs) noexcept { mValue += rhs.mValue; return *this; }
    constexpr ULOW& operator-=(const ULOW& rhs) noexcept { mValue -= rhs.mValue; return *this; }
    constexpr ULOW& operator*=(const ULOW& rhs) noexcept { mValue *= rhs.mValue; return *this; }

    // --- Деление и остаток ---
    [[nodiscard]] constexpr std::pair<ULOW, ULOW> operator/(const ULOW& rhs) const noexcept {
        return { ULOW{mValue / rhs.mValue}, ULOW{mValue % rhs.mValue} };
    }

    constexpr std::pair<ULOW, ULOW> operator/=(const ULOW& rhs) noexcept {
        auto res = *this / rhs;
        *this = res.first;
        return res;
    }

    // --- Взаимодействие с внешними типами (UBig) ---
    /**
         * @brief Оператор умножения (ULOW * UBig).
         * Использует концепт C++20, чтобы не перехватывать лишние типы.
         */
    template <typename T>
        requires requires(T t, ULOW u) { t * u; }
    [[nodiscard]] constexpr T operator*(const T& rhs) const {
        return rhs * (*this);
    }

    template <typename T>
    T& operator*=(const T&) = delete;

    // --- Утилиты ---
    [[nodiscard]] constexpr int countl_zero() const noexcept {
        return std::countl_zero(mValue);
    }

    [[nodiscard]] constexpr int mod10() const noexcept {
        return static_cast<int>(mValue % 10ull);
    }

    static constexpr ULOW get_max_value() noexcept {
        return ULOW{std::numeric_limits<uint64_t>::max()};
    }

private:
    uint64_t mValue{0};
};
}
