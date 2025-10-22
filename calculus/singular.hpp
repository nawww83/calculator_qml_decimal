/**
 * @author nawww83@gmail.com
 * @brief Класс для внедрения признака сингулярности числа: переполнение или признак "нечисло".
 */

#pragma once

#include <concepts>
#include <compare> // operator<=>

namespace singular
{

    /**
     * @brief Индикатор сингулярности числа: его переполнения или признака "нечисло".
     */
    template <std::unsigned_integral T>
    class Singular
    {
        /**
         * @brief Признак переполнения.
         */
        T mOverflow = 0;

        /**
         * @brief Признак "нечисло".
         */
        T mNaN = 0;

    public:
        /**
         * @brief Конструктор по умолчанию.
         */
        constexpr explicit Singular() = default;

        /**
         * @brief Конструктор с параметром.
         */
        constexpr explicit Singular(bool is_overflow) : mOverflow{is_overflow}, mNaN{false} {};

        /**
         * @brief Конструктор с параметрами.
         */
        constexpr explicit Singular(bool is_overflow, bool is_nan) : mOverflow{is_overflow}, mNaN{is_nan}
        {
            // Запрещаем состояние 1-1: признак "нечисло" побеждает переполнение.
            if (is_nan && is_overflow)
            {
                mOverflow = false;
            }
        };

        /**
         * @brief Конструктор копирования/перемещения.
         */
        constexpr Singular(const Singular &other) = default;
        constexpr Singular(Singular &&other) = default;

        /**
         * @brief Оператор копирования/перемещения.
         */
        constexpr Singular &operator=(const Singular &other) = default;
        constexpr Singular &operator=(Singular &&other) = default;

        /**
         * @brief Тестирует, есть ли какая-либо сингулярность.
         */
        bool operator()() const { return mNaN != 0 || mOverflow != 0; }

        /**
         * @brief Оператор сравнения. Считаем, что сингулярности априори несравнимы, поэтому возвращает "равно" <=> оба числа несингулярные (обычные).
         */
        bool operator==(const Singular &other) const
        {
            return !(mOverflow || other.mOverflow || mNaN || other.mNaN);
        }

        /**
         * @brief Остальные операторы, кроме оператора сравнения: удалены, потому что не определены (нет необходимости).
         */
        auto operator<=>(const Singular &other) const = delete;

        /**
         * @brief Признак переполнения.
         */
        bool is_overflow() const { return mOverflow != 0; }

        /**
         * @brief Признак "нечисло".
         */
        bool is_nan() const { return mNaN != 0; }

        /**
         * @brief
         */
        void set_nan() {
            mNaN = 1;
            mOverflow = 0;
        }

        /**
         * @brief
         */
        void set_overflow() {
            mNaN = 0;
            mOverflow = 1;
        }
    };

}