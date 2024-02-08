#pragma once

#include <cmath>
#include <cassert>
#include <cstring>
#include <array>


namespace dec_n {

inline int undigits(char d) {
    switch (d) {
    case '0':
        return 0;
        break;
    case '1':
        return 1;
        break;
    case '2':
        return 2;
        break;
    case '3':
        return 3;
        break;
    case '4':
        return 4;
        break;
    case '5':
        return 5;
        break;
    case '6':
        return 6;
        break;
    case '7':
        return 7;
        break;
    case '8':
        return 8;
        break;
    case '9':
        return 9;
        break;
    default:
        return 0;
    }
}

constexpr inline char digits[10] {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

inline int num_of_digits(long long x) {
    int i = 0;
    while (x) {
        x /= 10;
        i++;
    }
    return (i != 0) ? i : 1;
}

inline bool is_mult_overflow_ll(long long x, long long y) {
    if ((x != 0) && (y != 0)) {
        const long long z = (unsigned long long)(x)*(unsigned long long)(y);
        long long r1 = z / y;
        long long r2 = z / x;
        return (r1 != x) || (r2 != y);
    }
    return false;
}

inline bool is_add_overflow_ll(long long x, long long y) {
    constexpr int N = sizeof(unsigned long long) * CHAR_BIT;
    unsigned long long z = (unsigned long long)(x) + (unsigned long long)(y);
    return ((x < 0) && (y < 0) && (z <= ((1ull << (N-1))-1))) || (( (x > 0) && (y > 0) && (z > ((1ull << (N-1))-1))));
}

inline bool is_two_complement() {
    unsigned long long w = static_cast<unsigned long long>(-1ll);
    unsigned char b[8];
    std::memcpy(b, &w, 8);
    bool res = true;
    res &= (b[0] == 255u);
    res &= (b[1] == 255u);
    res &= (b[2] == 255u);
    res &= (b[3] == 255u);
    res &= (b[4] == 255u);
    res &= (b[5] == 255u);
    res &= (b[6] == 255u);
    res &= (b[7] == 255u);
    long long wi = static_cast<long long>(w);
    assert(wi == -1ll);
    return res && (wi == -1ll);
}

class Vector64 {
    static constexpr int _N = 31;
    int _size{};
    std::array<char, _N+1> buffer {}; // N+1 to be safe with the buffer
    int cast_size(int sz) const {
        return (sz > _N) ? _N : ((sz < 1) ? 0 : sz);
    }
    void fill_data(const char* b, int sz) {
        assert(sz >= 0);
        assert(sz <= _N);
        _size = std::min(_N, sz);
        for (int i=0; i<_size; ++i) {
            buffer[i] = *b++;
        }
        for (int i=0; i<(_N-_size); ++i) {
            buffer[_size + i] = '\0';
        }
        buffer[_N] = '\0';
    }
public:
    Vector64() = default;
    Vector64(const std::string& str) {
        fill_data(str.data(), str.size());
    }
    Vector64(std::string&& str) = delete;
    Vector64(const Vector64& other) {
        fill_data(other.buffer.data(), other.size());
    }
    Vector64(Vector64&& other) = delete;
    Vector64& operator=(const std::string& str) {
        fill_data(str.data(), str.size());
        return *this;
    }
    Vector64& operator=(const Vector64& other) {
        fill_data(other.buffer.data(), other.size());
        return *this;
    }
    char& operator[](int i) {
        assert(i >= 0);
        assert(i < _N);
        return buffer[i];
    }
    char operator[](int i) const {
        assert(i >= 0);
        assert(i < _N);
        return buffer[i];
    }
    int size() const {
        return _size;
    }
    static int N() {
        return _N;
    }
    void resize(int new_size) {
        _size = cast_size(new_size);
    }
    auto string_view() const {
        return std::string_view(buffer.data(), _size);
    }
};

template <int w=2> // precision
class Decimal {
    long long x0{}; // integer part
    long long x1{}; // nominator
    long long fraction{1}; // denominator
    Vector64 str{};
    void transform() { // transform any (x0, x1, fraction) to a string with fixed precision 'w' digits
        if (is_overflow()) {
            str = "";
            return;
        }
        long long r = x0;
        const int sign = is_negative();
        if (std::abs(x1) >= fraction) {
            r = (sign == 0) ? r + x1 / fraction : r - x1 / fraction;
            x1 = (x1 >= 0) ? x1 % fraction : -((-x1) % fraction);
        }
        // fraction part for print
        long long f = (std::pow(10ll, w) * (x1 < 0 ? -x1 : x1)) / fraction;
        const int len = (num_of_digits(r) + 1 + w) + ((sign == 0) ? 0 : 1); // sign, digits, separator, fraction
        assert(len <= Vector64::N());
        assert(len > 0);
        str.resize(len);
        if (sign != 0) {
            str[0] = '-';
        }
        if (r == 0) {
            str[len - w - 2] = '0';
        }
        r = (r < 0) ? -r : r;
        for (int i = 0; r != 0 ; i++) {
            str[len - w - 2 - i] = digits[r % 10ll];
            r /= 10ll;
        }
        str[len - 1 - w] = ','; // separator
        for (int i = 0; i < w; i++) {
            str[len - 1 - i] = digits[f % 10ll];
            f /= 10ll;
        }
        // std::cout << "after tr x0: " << x0 << ", x1: " << x1 << ", fr: " << fraction << "\n";
    }
    void untransform() { // convert fixed w-precision string to (x0, x1, fraction)
        if (str.size() < 1) {
            return;
        }
        const int sign = (str[0] == '-') ? 1 : 0;
        int i = (sign == 0) ? 0 : 1;
        x0 = (long long)undigits(str[i++]);
        while (((str[i] != ',') && (str[i] != '.')) && (str[i] != '\0')) {
            x0 *= 10ll;
            if (x0 < 0) {
                x0 = -1;
                x1 = -1;
                fraction = std::pow(10ll, w);
                return;
            }
            x0 += (long long)undigits(str[i++]);
        }
        x0 = (sign == 0) ? x0 : -x0;
        i++;
        fraction = std::pow(10ll, w);
        x1 = (long long)undigits(str[i++]);
        const int len = str.size();
        int j = 1;
        while (i < len) {
            if (j >= w) { // too many of digits after the comma
                break;
            }
            x1 *= 10ll;
            x1 += (long long)undigits(str[i++]);
            j++;
        }
        while (j < w) { // example: 4,5 => 4,50 if w = 2
            x1 *= 10ll;
            x1 += 0ll;
            j++;
        }
        if ((x0 == 0) && (sign != 0)) {
            x1 = -x1;
        }
    }
public:
    Decimal() = default;
    void set(long long x, long long y, long long f) {
        x0 = x;
        x1 = y;
        fraction = f;
        if ((f <= 0) || ((x < 0) && (y < 0))) {
            return;
        }
        assert(((y >= 0) && (x != 0)) || (x == 0));
        assert(f > 0);
        // do forward-back transformation to get fixed denominator (fraction) with w digits
        transform();
        untransform();
    }
    bool is_an_integer() const {
        return (x1 == 0);
    }
    bool is_overflow() const {
        return ((fraction <= 0) || ((x0 < 0) && (x1 < 0)));
    }
    bool is_strong_negative() const {
        return (x0 < 0);
    }
    bool is_weak_negative() const {
        return ((x0 == 0) && (x1 < 0));
    }
    bool is_negative() const {
        // return (x0 < 0) || ((x0 == 0) && (x1 < 0));
        return is_strong_negative() || is_weak_negative();
    }
    bool is_zero() const {
        return (x0 == 0) && (x1 == 0);
    }
    auto value() const {
        return str.string_view();
    }
    double value_as_double() const {
        auto res = (x0 != 0) ? double(x0) + ((x0 < 0) ? -1. : 1.)*double(x1)/double(fraction) : double(x1)/double(fraction);
        return res;
    }
    auto integer_part() const {
        return x0;
    }
    auto nominator() const {
        return x1;
    }
    auto denominator() const {
        return fraction;
    }
    void set_str(std::string& s) {
        str = Vector64(s);
        if (s.size() < 1) {
            return;
        }
        untransform();
        transform();
    }
    Decimal operator+(const Decimal& other) {
        Decimal res{};
        const int neg1 = is_negative();
        const int neg2 = other.is_negative();
        bool is_overflow1 = is_add_overflow_ll(x0, other.x0);
        bool is_overflow2 = is_add_overflow_ll(x1, other.x1);
        if (is_overflow1 || is_overflow2) {
            res.set(-1, -1, fraction);
            return res;
        }
        auto sum = x0 + other.x0;
        const bool equal_fractions = (fraction == other.fraction);
        // Either all are positives or all are negatives: use abs()
        auto f = (equal_fractions) ? std::abs(x1) + std::abs(other.x1) : std::abs(x1)*other.fraction + std::abs(other.x1)*fraction;
        const bool differ_signs = (neg1 ^ neg2);
        if (neg1 && !neg2) {
            f = (equal_fractions) ? -std::abs(x1) + std::abs(other.x1) : -std::abs(x1)*other.fraction + std::abs(other.x1)*fraction;
        }
        if (!neg1 && neg2) {
            f = (equal_fractions) ? std::abs(x1) - std::abs(other.x1) : std::abs(x1)*other.fraction - std::abs(other.x1)*fraction;
        }
        if (differ_signs) {
            if ((f < 0) && (sum < 0)) {
                f = -f;
            } else
                if ((f < 0) && (sum > 0)) {
                    f += (equal_fractions) ? fraction : other.fraction*fraction;
                    sum -= 1;
                } else
                    if ((f > 0) && (sum < 0)) {
                        f -= (equal_fractions) ? fraction : other.fraction*fraction;
                        sum += 1;
                        if (sum != 0) {
                            f = std::abs(f);
                        }
                    }
        }
        if (neg1 && neg2) {
            if (sum == 0) {
                f = -f;
            }
        }
        res.set(sum, f, (equal_fractions) ? fraction : other.fraction*fraction);
        return res;
    }
    Decimal operator-(const Decimal& other) {
        Decimal res{};
        res.set(-other.x0, (other.x0 != 0) ? other.x1 : -other.x1, other.fraction);
        return res + *this;
    }
    Decimal operator*(const Decimal& other) {
        Decimal res{};
        const bool neg1 = is_negative();
        const bool neg2 = other.is_negative();
        bool overflow1 = is_mult_overflow_ll(x0, other.x1);
        bool overflow2 = is_mult_overflow_ll(other.x0, x1) ;
        bool overflow3 = is_mult_overflow_ll(x0, x1);
        bool overflow4 = is_mult_overflow_ll(other.x0, other.x1);
        bool overflow5 = is_mult_overflow_ll(x1, other.x1);
        bool overflow6 = is_mult_overflow_ll(x0, other.x0);
        const bool equal_fractions = (fraction == other.fraction);
        const bool all_integers = ((x1 == 0) && (other.x1 == 0));
        if (overflow6) {
            res.set(-1, -1, fraction);
            return res;
        }
        auto prod = x0*other.x0;
        auto f = prod*0;
        if (all_integers && equal_fractions) {
            res.set(prod, f, fraction);
            return res;
        }
        const bool left_integer = ((x1 == 0) && (other.x1 != 0));
        if (left_integer && equal_fractions) {
            if (overflow1) {
                res.set(-1, -1, fraction);
                return res;
            }
            prod += ((neg1 ^ neg2) ? -1ll : 1ll) * (std::abs(x0)*std::abs(other.x1)) / fraction;
            f = (std::abs(x0)*std::abs(other.x1)) % fraction;
            if (neg1 ^ neg2) {
                f = (prod == 0) ? -f : f;
            }
            res.set(prod, f, fraction);
            return res;
        }
        const bool right_integer = ((x1 != 0) && (other.x1 == 0));
        if (right_integer && equal_fractions) {
            if (overflow2) {
                res.set(-1, -1, fraction);
                return res;
            }
            prod += ((neg1 ^ neg2) ? -1ll : 1ll) * (std::abs(x1)*std::abs(other.x0)) / fraction;
            f = (std::abs(x1)*std::abs(other.x0)) % fraction;
            if (neg1 ^ neg2) {
                f = (prod == 0) ? -f : f;
            }
            res.set(prod, f, fraction);
            return res;
        }
        if (overflow1 || overflow2 || overflow3 || overflow4 || overflow5 || overflow6) {
            res.set(-1, -1, fraction);
            return res;
        }
        if (!neg1 && !neg2) { // both are non-negatives
            if (equal_fractions) { // checked
                prod += (x0*other.x1 + x1*other.x0 + (x1*other.x1)/fraction) / fraction;
                f = (x0*other.x1 + x1*other.x0 + (x1*other.x1)/fraction) % fraction;
            } else {
                prod += (x0*other.x1*fraction + x1*other.x0*other.fraction + x1*other.x1) / (other.fraction*fraction);
                f = (x0*other.x1*fraction + x1*other.x0*other.fraction + x1*other.x1) % (other.fraction*fraction);
            }
        }
        if (neg1 && neg2) { // both negative
            const int neg1_strong = is_strong_negative();
            const int neg2_strong = other.is_strong_negative();
            const int neg1_weak = is_weak_negative();
            const int neg2_weak = other.is_weak_negative();
            if (neg1_strong && neg2_strong) {
                if (equal_fractions) {
                    prod += (std::abs(x0)*other.x1 + std::abs(other.x0)*x1 + (x1*other.x1)/fraction) / (fraction);
                    f = (std::abs(x0)*other.x1 + std::abs(other.x0)*x1 + (x1*other.x1)/fraction) % (fraction);
                } else {
                    prod += (std::abs(x0)*fraction*other.x1 + std::abs(other.x0)*other.fraction*x1 + x1*other.x1) / (other.fraction*fraction);
                    f = (std::abs(x0)*fraction*other.x1 + std::abs(other.x0)*other.fraction*x1 + x1*other.x1) % (other.fraction*fraction);
                }
            }
            if (neg1_weak && neg2_strong) {
                if (equal_fractions) {
                    prod += (std::abs(other.x0)*std::abs(x1) + (std::abs(x1)*other.x1)/fraction) / (fraction);
                    f = (std::abs(other.x0)*std::abs(x1) + (std::abs(x1)*other.x1)/fraction) % (fraction);
                } else {
                    prod += (std::abs(other.x0)*other.fraction*std::abs(x1) + std::abs(x1)*other.x1) / (other.fraction*fraction);
                    f = (std::abs(other.x0)*other.fraction*std::abs(x1) + std::abs(x1)*other.x1) % (other.fraction*fraction);
                }
            }
            if (neg1_strong && neg2_weak) {
                if (equal_fractions) {
                    prod += (std::abs(x0)*std::abs(other.x1) + (x1*std::abs(other.x1))/fraction) / (fraction);
                    f = (std::abs(x0)*std::abs(other.x1) + (x1*std::abs(other.x1))/fraction) % (fraction);
                } else {
                    prod += (std::abs(other.x1)*(std::abs(x0)*fraction + x1)) / (other.fraction*fraction);
                    f = (std::abs(other.x1)*(std::abs(x0)*fraction + x1)) % (other.fraction*fraction);
                }
            }
            if (neg1_weak && neg2_weak) {
                if (equal_fractions) {
                    prod += ((std::abs(x1)*std::abs(other.x1))/fraction) / (fraction);
                    f = ((std::abs(x1)*std::abs(other.x1))/fraction) % (fraction);
                } else {
                    prod += (std::abs(other.x1)*std::abs(x1)) / (other.fraction*fraction);
                    f = (std::abs(other.x1)*std::abs(x1)) % (other.fraction*fraction);
                }
            }
        }
        // one positive, other - negative
        if (neg1 && !neg2) {
            const int neg1_strong = is_strong_negative();
            const int neg1_weak = is_weak_negative();
            if (neg1_strong) { // checked
                if (equal_fractions) {
                    prod = std::abs(prod) + (std::abs(x0)*other.x1 + other.x0*x1 + (x1*other.x1)/fraction) / (fraction);
                    f = (std::abs(x0)*other.x1 + other.x0*x1 + (x1*other.x1)/fraction) % (fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                } else {
                    prod = std::abs(prod) + (std::abs(x0)*fraction*other.x1 + other.x0*other.fraction*x1 + x1*other.x1) / (other.fraction*fraction);
                    f = (std::abs(x0)*fraction*other.x1 + other.x0*other.fraction*x1 + x1*other.x1) % (other.fraction*fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                }
            }
            if (neg1_weak) { // checked
                if (equal_fractions) {
                    prod = (other.x0*std::abs(x1) + (std::abs(x1)*other.x1)/fraction) / (fraction);
                    f = (other.x0*std::abs(x1) + (std::abs(x1)*other.x1)/fraction) % (fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                } else {
                    prod = (std::abs(x1)*(other.x0*other.fraction + other.x1)) / (other.fraction*fraction);
                    f = (std::abs(x1)*(other.x0*other.fraction + other.x1)) % (other.fraction*fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                }
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.is_strong_negative();
            const int neg2_weak = other.is_weak_negative();
            if (neg2_strong) { // checked
                if (equal_fractions) {
                    prod = std::abs(prod) + (x0*other.x1 + std::abs(other.x0)*x1 + (x1*other.x1)/fraction) / (fraction);
                    f = (x0*other.x1 + std::abs(other.x0)*x1 + (x1*other.x1)/fraction) % (fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                } else {
                    prod = std::abs(prod) + (x0*fraction*other.x1 + std::abs(other.x0)*other.fraction*x1 + x1*other.x1) / (other.fraction*fraction);
                    f = (x0*fraction*other.x1 + std::abs(other.x0)*other.fraction*x1 + x1*other.x1) % (other.fraction*fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                }
            }
            if (neg2_weak) { // checked
                if (equal_fractions) {
                    prod = (x0*std::abs(other.x1) + (x1*std::abs(other.x1))/fraction) / (fraction);
                    f = (x0*std::abs(other.x1) + (x1*std::abs(other.x1))/fraction) % (fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                } else {
                    prod = (std::abs(other.x1)*(x0*fraction + x1)) / (other.fraction*fraction);
                    f = (std::abs(other.x1)*(x0*fraction + x1)) % (other.fraction*fraction);
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                }
            }
        }
        res.set(prod, f, (equal_fractions) ? fraction : other.fraction*fraction);
        return res;
    }
    Decimal operator/(const Decimal& other) {
        Decimal res{};
        const bool neg1 = is_negative();
        const bool neg2 = other.is_negative();
        const bool equal_fractions = (fraction == other.fraction);
        const bool all_integers = ((x1 == 0) && (other.x1 == 0));
        if (all_integers && equal_fractions) {
            auto prod = std::abs(x0) / std::abs(other.x0);
            auto f = std::abs(x0) % std::abs(other.x0);
            if (neg1 ^ neg2) {
                prod = (prod != 0) ? -prod : prod;
                f = (prod == 0) ? -f : f;
            }
            res.set(prod, f, std::abs(other.x0));
            return res;
        }
        const bool denominator_is_integer = (other.x1 == 0) && (other.x0 != 0);
        if ((denominator_is_integer) && (equal_fractions)) { // experimental: not tested
            auto prod = std::abs(x0) / std::abs(other.x0) + (std::abs(x0) % std::abs(other.x0)) / std::abs(other.x0);
            // auto f = ((std::abs(x0) % std::abs(other.x0)) * fraction) / std::abs(other.x0) +
                     // std::abs(x1) / std::abs(other.x0);
            auto f = (std::abs(x1) + (std::abs(x0) % std::abs(other.x0)) * fraction) / std::abs(other.x0);
            if (neg1 ^ neg2) {
                prod = (prod != 0) ? -prod : prod;
                f = (prod == 0) ? -f : f;
            }
            res.set(prod, f, fraction);
            return res;
        }
        if ( is_mult_overflow_ll(x0, fraction) ) {
            res.set(-1, -1, fraction);
            return res;
        }
        if (!neg1 && !neg2) {
            if (equal_fractions) {
                auto prod = (x0*fraction + x1) / (other.x0*fraction + other.x1);
                auto f = (x0*fraction + x1) % (other.x0*fraction + other.x1);
                res.set(prod, f, other.x0*fraction + other.x1);
            } else {
                auto prod = (x0*fraction + x1) / (other.x0*other.fraction + other.x1);
                auto f = (x0*fraction + x1) % (other.x0*other.fraction + other.x1);
                res.set(prod, f, other.x0*other.fraction + other.x1);
                Decimal tmp;
                tmp.set(0, other.fraction, fraction);
                return res * tmp;
            }
        }
        if (neg1 && neg2) {
            const int neg1_strong = is_strong_negative();
            const int neg2_strong = other.is_strong_negative();
            const int neg1_weak = is_weak_negative();
            const int neg2_weak = other.is_weak_negative();
            if (neg1_strong && neg2_strong) {
                if (equal_fractions) {
                    auto prod = (std::abs(x0)*fraction + x1) / (std::abs(other.x0)*fraction + other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % (std::abs(other.x0)*fraction + other.x1);
                    res.set(prod, f, std::abs(other.x0)*fraction + other.x1);
                } else {
                    auto prod = (std::abs(x0)*fraction + x1) / (std::abs(other.x0)*other.fraction + other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % (std::abs(other.x0)*other.fraction + other.x1);
                    res.set(prod, f, std::abs(other.x0)*other.fraction + other.x1);
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
            if (neg1_weak && neg2_weak) {
                if (equal_fractions) {
                    auto prod = x1 / other.x1;
                    auto f = std::abs(x1) % std::abs(other.x1);
                    res.set(prod, f, std::abs(other.x1));
                } else {
                    auto prod = x1 / other.x1;
                    auto f = std::abs(x1) % std::abs(other.x1);
                    res.set(prod, f, std::abs(other.x1));
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
            if (neg1_strong && neg2_weak) {
                if (equal_fractions) {
                    auto prod = (std::abs(x0)*fraction + x1) / std::abs(other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % std::abs(other.x1);
                    res.set(prod, f, std::abs(other.x1));
                } else {
                    auto prod = (std::abs(x0)*fraction + x1) / std::abs(other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % std::abs(other.x1);
                    res.set(prod, f, std::abs(other.x1));
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
            if (neg1_weak && neg2_strong) {
                if (equal_fractions) {
                    auto prod = std::abs(x1) / (std::abs(other.x0)*fraction + other.x1);
                    auto f = std::abs(x1) % (std::abs(other.x0)*fraction + other.x1);
                    res.set(prod, f, std::abs(other.x0)*fraction + other.x1);
                } else {
                    auto prod = std::abs(x1) / (std::abs(other.x0)*other.fraction + other.x1);
                    auto f = std::abs(x1) % (std::abs(other.x0)*other.fraction + other.x1);
                    res.set(prod, f, std::abs(other.x0)*other.fraction + other.x1);
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
        }
        if (neg1 && !neg2) {
            const int neg1_strong = is_strong_negative();
            const int neg1_weak = is_weak_negative();
            if (neg1_strong) {
                if (equal_fractions) {
                    auto prod = (std::abs(x0)*fraction + x1) / (other.x0*fraction + other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % (other.x0*fraction + other.x1);
                    // std::cout << " 1f: " << f << ", den: " << other.x0*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, other.x0*fraction + other.x1);
                } else {
                    auto prod = (std::abs(x0)*fraction + x1) / (other.x0*other.fraction + other.x1);
                    auto f = (std::abs(x0)*fraction + x1) % (other.x0*other.fraction + other.x1);
                    // std::cout << " 2f: " << f << ", den: " << other.x0*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, other.x0*other.fraction + other.x1);
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
            if (neg1_weak) {
                if (equal_fractions) {
                    auto prod = std::abs(x1) / (other.x0*fraction + other.x1);
                    auto f = std::abs(x1) % (other.x0*fraction + other.x1);
                    // std::cout << " 3f: " << f << ", den: " << other.x0*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, other.x0*fraction + other.x1);
                } else {
                    auto prod = std::abs(x1) / (other.x0*other.fraction + other.x1);
                    auto f = std::abs(x1) % (other.x0*other.fraction + other.x1);
                    // std::cout << " 4f: " << f << ", den: " << other.x0*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, other.x0*other.fraction + other.x1);
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
        }
        if (!neg1 && neg2) {
            const int neg2_strong = other.is_strong_negative();
            const int neg2_weak = other.is_weak_negative();
            if (neg2_strong) {
                if (equal_fractions) {
                    auto prod = (x0*fraction + x1) / (std::abs(other.x0)*fraction + other.x1);
                    auto f = (x0*fraction + x1) % (std::abs(other.x0)*fraction + other.x1);
                    // std::cout << " 5f: " << f << ", den: " << std::abs(other.x0)*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, std::abs(other.x0)*fraction + other.x1);
                } else {
                    auto prod = (x0*fraction + x1) / (std::abs(other.x0)*other.fraction + other.x1);
                    auto f = (x0*fraction + x1) % (std::abs(other.x0)*other.fraction + other.x1);
                    // std::cout << " 6f: " << f << ", den: " << std::abs(other.x0)*other.fraction + other.x1 << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, std::abs(other.x0)*other.fraction + other.x1);
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
            if (neg2_weak) {
                if (equal_fractions) {
                    auto prod = (x0*fraction + x1) / std::abs(other.x1);
                    auto f = (x0*fraction + x1) % std::abs(other.x1);
                    // std::cout << " 7f: " << f << ", den: " << std::abs(other.x1) << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, std::abs(other.x1));
                } else {
                    auto prod = (x0*fraction + x1) / std::abs(other.x1);
                    auto f = (x0*fraction + x1) % std::abs(other.x1);
                    // std::cout << " 8f: " << f << ", den: " << std::abs(other.x1) << ", prod: " << prod << "\n";
                    prod = -prod;
                    f = (prod == 0) ? -f : f;
                    res.set(prod, f, std::abs(other.x1));
                    Decimal tmp;
                    tmp.set(0, other.fraction, fraction);
                    return res * tmp;
                }
            }
        }
        return res;
    }
};

bool inline operator==(const Decimal<>& lhs, const Decimal<>& rhs) {
    return lhs.value() == rhs.value();
}

}
