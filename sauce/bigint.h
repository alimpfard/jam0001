#pragma once

#include <AK/UFixedBigInt.h>

template<typename Inner>
class FixedBigInt {
public:
    enum class Sign {
        Positive,
        Negative,
    };

    constexpr FixedBigInt() = default;
    constexpr FixedBigInt(Inner v, Sign sign)
        : m_sign(sign)
        , m_value(move(v))
    {
    }

    constexpr FixedBigInt(Inner v, UnderlyingType<Sign> sign)
        : m_sign(static_cast<Sign>(sign))
        , m_value(move(v))
    {
    }

    constexpr FixedBigInt(u64 v)
        : m_sign(Sign::Positive)
        , m_value(v)
    {
    }

    constexpr FixedBigInt(i64 v)
        : m_sign(v >= 0 ? Sign::Positive : Sign::Negative)
        , m_value((u64)AK::abs(v))
    {
    }

    constexpr FixedBigInt(bool v)
        : m_sign(Sign::Positive)
        , m_value((u64)v)
    {
    }

    explicit operator bool() const { return m_value.operator bool(); }

    [[nodiscard]] i64 value() const
    {
        auto value = m_value.low();
        if constexpr (IsSame<u64, decltype(value)>) {
            if (m_sign == Sign::Negative)
                return -value;
            return value;
        } else {
            return FixedBigInt<decltype(value)>(value, to_underlying(m_sign)).value();
        }
    }

    template<typename T>
    constexpr auto operator+(T&& other) const
    {
        if (other.m_value == 0u)
            return *this;

        if (other.m_sign == Sign::Positive && m_sign == Sign::Positive)
            return FixedBigInt<Inner>(m_value + other.m_value, Sign::Positive);

        if (other.m_sign == Sign::Positive) {
            if (!(*this > other))
                return FixedBigInt<Inner>(m_value - other.m_value, Sign::Negative);
            return FixedBigInt<Inner>(m_value - other.m_value, Sign::Positive);
        }

        return FixedBigInt<Inner>(m_value + other.m_value, Sign::Negative);
    }

    constexpr auto operator-(auto other) const
    {
        return *this + (-other);
    }

    constexpr auto operator-() const
    {
        return FixedBigInt<Inner>(m_value, m_sign == Sign::Positive ? Sign::Negative : Sign::Positive);
    }

    constexpr auto operator*(auto other) const
    {
        if (other.m_value == 0u)
            return FixedBigInt<Inner>();

        auto sign = (m_sign == Sign::Positive) ^ (other.m_sign == Sign::Positive);
        return FixedBigInt<Inner>(m_value * other.m_value, sign ? Sign::Negative : Sign::Positive);
    }

    constexpr auto operator/(auto other) const
    {
        auto sign = (m_sign == Sign::Positive) ^ (other.m_sign == Sign::Positive);
        return FixedBigInt<Inner>(m_value / other.m_value, sign ? Sign::Negative : Sign::Positive);
    }

    constexpr auto operator%(auto other) const
    {
        auto sign = (m_sign == Sign::Positive) ^ (other.m_sign == Sign::Positive);
        return FixedBigInt<Inner>(m_value % other.m_value, sign ? Sign::Negative : Sign::Positive);
    }

    template<typename T>
    constexpr auto operator>(T&& other) const
    {
        if (other.m_value == 0u)
            return m_sign == Sign::Positive;

        if (m_sign == Sign::Negative && other.m_sign == Sign::Positive)
            return false;

        auto value_gt = m_value > other.m_value;

        if (m_sign == Sign::Positive && other.m_sign == Sign::Positive)
            return value_gt;

        return !value_gt;
    }

    template<typename T>
    constexpr auto operator<(T&& other) const
    {
        return !(*this > other) && !(*this == other);
    }

    template<typename T>
    constexpr auto operator==(T&& other) const
    {
        return m_sign == other.m_sign && m_value == other.m_value;
    }

    bool positive() const { return m_sign == Sign::Positive; }
    auto const& absolute() const { return m_value; }

private:
    Sign m_sign { Sign::Positive };
    Inner m_value;
};

template<typename T>
struct AK::Formatter<FixedBigInt<T>> : public AK::Formatter<T> {
    void format(FormatBuilder& builder, auto& value)
    {
        if (!value.positive())
            builder.put_string("-");

        Formatter<T>{}.format(builder, value.absolute());
    }
};
