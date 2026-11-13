#include "rental/Money.h"

#include "rental/Errors.h"

#include <cctype>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace rental {

std::int64_t divideRounded(std::int64_t numerator, std::int64_t denominator) {
    if (denominator <= 0) {
        throw std::invalid_argument("divideRounded: denominator must be positive");
    }
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

BasisPoints multiplyBp(BasisPoints a, BasisPoints b) {
    return divideRounded(a * b, kBpScale);
}

namespace {
// Prints value / 10000 with at least two and at most four decimals.
std::string formatScaled(std::int64_t value) {
    const bool negative = value < 0;
    const std::int64_t abs = negative ? -value : value;
    std::string fraction = std::to_string(abs % kBpScale);
    fraction.insert(0, 4 - fraction.size(), '0');
    while (fraction.size() > 2 && fraction.back() == '0') {
        fraction.pop_back();
    }
    return std::string(negative ? "-" : "") + std::to_string(abs / kBpScale) + "." + fraction;
}
} // namespace

std::string formatFactor(BasisPoints bp) {
    return formatScaled(bp) + "x";
}

std::string formatPercent(BasisPoints bp) {
    // bp / 100 = percent
    std::string s = std::to_string(bp / 100);
    const std::int64_t rest = (bp < 0 ? -bp : bp) % 100;
    if (rest != 0) {
        std::string frac = std::to_string(rest);
        frac.insert(0, 2 - frac.size(), '0');
        if (frac.back() == '0') frac.pop_back();
        s += "." + frac;
    }
    return s + "%";
}

Money Money::parse(const std::string& raw) {
    std::string text;
    for (char c : raw) {
        if (!std::isspace(static_cast<unsigned char>(c)) && c != ',') text += c;
    }
    if (text.size() >= 3 && (text.compare(0, 3, "INR") == 0 || text.compare(0, 3, "inr") == 0)) {
        text.erase(0, 3);
    }
    if (text.empty()) throw ValidationError("Amount is empty.");

    std::int64_t rupees = 0;
    std::int64_t paise = 0;
    std::size_t i = 0;
    bool sawDigit = false;
    for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])); ++i) {
        sawDigit = true;
        if (rupees > std::numeric_limits<std::int64_t>::max() / 1000) {
            throw ValidationError("Amount is too large: " + raw);
        }
        rupees = rupees * 10 + (text[i] - '0');
    }
    if (i < text.size()) {
        if (text[i] != '.') throw ValidationError("Not a valid amount: '" + raw + "' (use e.g. 1500 or 1500.50)");
        ++i;
        int decimals = 0;
        for (; i < text.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(text[i])) || decimals == 2) {
                throw ValidationError("Not a valid amount: '" + raw + "' (at most two decimals)");
            }
            paise = paise * 10 + (text[i] - '0');
            ++decimals;
            sawDigit = true;
        }
        if (decimals == 1) paise *= 10;
    }
    if (!sawDigit) throw ValidationError("Not a valid amount: '" + raw + "'");
    return Money(rupees * 100 + paise);
}

Money Money::applyBasisPoints(BasisPoints bp) const {
    return Money(divideRounded(paise_ * bp, kBpScale));
}

std::string Money::toString() const {
    const bool negative = paise_ < 0;
    const std::int64_t abs = negative ? -paise_ : paise_;
    const std::string digits = std::to_string(abs / 100);

    // Indian grouping: last three digits, then groups of two (1,50,000).
    std::string grouped;
    const std::size_t n = digits.size();
    if (n <= 3) {
        grouped = digits;
    } else {
        const std::string last3 = digits.substr(n - 3);
        std::string head = digits.substr(0, n - 3);
        std::string headGrouped;
        while (head.size() > 2) {
            headGrouped = "," + head.substr(head.size() - 2) + headGrouped;
            head.erase(head.size() - 2);
        }
        grouped = head + headGrouped + "," + last3;
    }
    std::string cents = std::to_string(abs % 100);
    if (cents.size() < 2) cents.insert(0, "0");
    return std::string(negative ? "-" : "") + "INR " + grouped + "." + cents;
}

std::ostream& operator<<(std::ostream& os, Money m) {
    return os << m.toString();
}

} // namespace rental
