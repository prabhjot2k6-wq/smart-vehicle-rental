#include "rental/Date.h"

#include "rental/Errors.h"

#include <cctype>
#include <ostream>

namespace rental {

namespace {

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant).
long long daysFromCivil(int y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;                                  // [0, 399]
    const long long mp = m > 2 ? m - 3 : m + 9;                           // March = 0
    const long long doy = (153 * mp + 2) / 5 + d - 1;                     // [0, 365]
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return era * 146097 + doe - 719468;
}

struct Ymd { int y; int m; int d; };

Ymd civilFromDays(long long z) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const int y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
    return {y, m, d};
}

std::string pad(int value, int width) {
    std::string s = std::to_string(value);
    if (static_cast<int>(s.size()) < width) s.insert(0, static_cast<std::size_t>(width) - s.size(), '0');
    return s;
}

} // namespace

bool Date::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int Date::daysInMonth(int year, int month) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    return (month == 2 && isLeapYear(year)) ? 29 : kDays[month - 1];
}

bool Date::isValid(int year, int month, int day) {
    return year >= 1900 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 &&
           day <= daysInMonth(year, month);
}

Date Date::fromYmd(int year, int month, int day) {
    if (!isValid(year, month, day)) {
        throw ValidationError("Not a real calendar date: " + pad(year, 4) + "-" + pad(month, 2) + "-" +
                              pad(day, 2));
    }
    return Date(daysFromCivil(year, month, day));
}

Date Date::parse(const std::string& text) {
    const auto bad = [&text]() {
        return ValidationError("Date must be in YYYY-MM-DD format, e.g. 2026-11-25 (got '" + text + "').");
    };
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') throw bad();
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) throw bad();
    }
    const int y = std::stoi(text.substr(0, 4));
    const int m = std::stoi(text.substr(5, 2));
    const int d = std::stoi(text.substr(8, 2));
    return fromYmd(y, m, d);
}

int Date::year() const { return civilFromDays(days_).y; }
int Date::month() const { return civilFromDays(days_).m; }
int Date::day() const { return civilFromDays(days_).d; }

int Date::weekdayIndex() const {
    // 1970-01-01 was a Thursday (index 4).
    const long long w = (days_ + 4) % 7;
    return static_cast<int>(w < 0 ? w + 7 : w);
}

std::string Date::weekdayName() const {
    static const char* kNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return kNames[weekdayIndex()];
}

bool Date::isWeekend() const {
    const int w = weekdayIndex();
    return w == 0 || w == 6;
}

std::string Date::toString() const {
    const Ymd ymd = civilFromDays(days_);
    return pad(ymd.y, 4) + "-" + pad(ymd.m, 2) + "-" + pad(ymd.d, 2);
}

std::ostream& operator<<(std::ostream& os, Date d) {
    return os << d.toString();
}

DateRange::DateRange(Date start, Date end) : start_(start), end_(end) {
    if (end <= start) {
        throw ValidationError("Return date (" + end.toString() + ") must be after pickup date (" +
                              start.toString() + ").");
    }
}

std::vector<Date> DateRange::days() const {
    std::vector<Date> result;
    result.reserve(static_cast<std::size_t>(lengthDays()));
    for (Date d = start_; d < end_; d = d.addDays(1)) result.push_back(d);
    return result;
}

std::string DateRange::toString() const {
    const long long n = lengthDays();
    return start_.toString() + " -> " + end_.toString() + " (" + std::to_string(n) + (n == 1 ? " day)" : " days)");
}

} // namespace rental
