/**
 * @author nawww83@gmail.com
 * @brief Класс для внедрения знака в числа.
 */

#pragma once

#include <concepts>
#include <compare> // operator<=>

namespace sign
{

    /**
     * @brief Знаковый манипулятор.
     */
    template <std::unsigned_integral T>
    class Sign
    {
        /**
         * @brief Информация о знаке:
         *  0 - знака нет,
         *  Не равно 0 - знак есть.
         */
        T mSign = 0;

    public:
        /**
         * @brief Конструктор по умолчанию.
         */
        constexpr explicit Sign() = default;

        /**
         * @brief Конструкторы копирования/перемещения.
         */
        constexpr Sign(const Sign &other) = default;
        constexpr Sign(Sign &&other) = default;

        /**
         * @brief Конструктор с параметром.
         */
        constexpr Sign(bool value) : mSign{value} {};

        /**
         * @brief Операторы копирования/перемещения.
         */
        constexpr Sign &operator=(const Sign &other) = default;
        constexpr Sign &operator=(Sign &&other) = default;

        /**
         * @brief
         */
        Sign operator^(const Sign& rhs) const
        {
            Sign result = *this;
            result.set_sign( *this != rhs );
            return result;
        }

        /**
         * @brief
         */
        Sign& operator^=(const Sign& rhs)
        {
            *this = *this ^ rhs;
            return *this;
        }

        /**
         * @brief Тестирует, знак есть/нет.
         */
        bool operator()() const { return mSign != 0; }

        /**
         * @brief Изменяет знак на противоположный.
         */
        void operator-()
        {
            mSign = 1 - operator()();
        }

        /**
         * @brief Оператор сравнения.
         */
        bool operator==(const Sign &other) const
        {
            return (operator()() && other.operator()()) || (!operator()() && !other.operator()());
        }

        /**
         * @brief Остальные операторы, кроме оператора сравнения: удалены, потому что не определены (нет необходимости).
         */
        auto operator<=>(const Sign &other) = delete;

        /**
         * @brief Установить знак:
         *  1 - знак "минус".
         *  0 - знак "плюс".
         */
        void set_sign(bool sign) {
            mSign = sign;
        }
    };

}