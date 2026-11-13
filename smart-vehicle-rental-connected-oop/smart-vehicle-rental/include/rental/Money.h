#pragma once
// Money and basis-point arithmetic.
//
// WHY INTEGERS: binary floating point cannot represent 0.10 or 1.15 exactly,
// so sums of doubles drift (0.1 + 0.2 != 0.3). All money is stored as a whole
// number of paise (1 rupee = 100 paise) in a 64-bit integer.
//
// MULTIPLIERS AND PERCENTAGES are stored as integer *basis points*:
//   10000 bp = 1.00x = 100%     11500 bp = 1.15x     1000 bp = 10%
//
// ROUNDING RULE (used everywhere): round half away from zero to the nearest
// paisa (or nearest basis point). Example: 500.5 paise -> 501 paise.

#include <cstdint>
#include <iosfwd>
#include <string>

namespace rental {

using BasisPoints = std::int64_t;
constexpr BasisPoints kBpScale = 10000; // 1.00x

// numerator / denominator, rounding half away from zero. denominator must be > 0.
std::int64_t divideRounded(std::int64_t numerator, std::int64_t denominator);

// (a x b) for two basis-point factors, e.g. 1.25 x 1.15 = 1.4375 -> 14375 bp.
BasisPoints multiplyBp(BasisPoints a, BasisPoints b);

// "1.15x", "1.4375x"
std::string formatFactor(BasisPoints bp);
// "10%", "12.5%"
std::string formatPercent(BasisPoints bp);

class Money {
public:
    constexpr Money() = default;

    static constexpr Money fromPaise(std::int64_t paise) { return Money(paise); }
    static constexpr Money fromRupees(std::int64_t rupees) { return Money(rupees * 100); }

    // Accepts "1500", "1500.5", "1,500.50", optionally prefixed with "INR".
    // Rejects negatives, more than two decimals and junk. Throws ValidationError.
    static Money parse(const std::string& text);

    constexpr std::int64_t paise() const { return paise_; }
    bool isZero() const { return paise_ == 0; }
    bool isNegative() const { return paise_ < 0; }

    // Multiply by a basis-point factor using the project rounding rule.
    Money applyBasisPoints(BasisPoints bp) const;

    Money operator+(Money other) const { return Money(paise_ + other.paise_); }
    Money operator-(Money other) const { return Money(paise_ - other.paise_); }
    Money operator-() const { return Money(-paise_); }
    Money operator*(std::int64_t count) const { return Money(paise_ * count); }
    Money& operator+=(Money other) { paise_ += other.paise_; return *this; }
    Money& operator-=(Money other) { paise_ -= other.paise_; return *this; }

    bool operator==(Money o) const { return paise_ == o.paise_; }
    bool operator!=(Money o) const { return paise_ != o.paise_; }
    bool operator<(Money o) const { return paise_ < o.paise_; }
    bool operator<=(Money o) const { return paise_ <= o.paise_; }
    bool operator>(Money o) const { return paise_ > o.paise_; }
    bool operator>=(Money o) const { return paise_ >= o.paise_; }

    // Consistent display format with Indian digit grouping: "INR 1,50,000.00".
    std::string toString() const;

private:
    explicit constexpr Money(std::int64_t paise) : paise_(paise) {}
    std::int64_t paise_ = 0;
};

std::ostream& operator<<(std::ostream& os, Money m);

} // namespace rental
