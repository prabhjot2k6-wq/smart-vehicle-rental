#pragma once
// Date-only calendar arithmetic and half-open rental periods.
//
// RENTAL DATE MODEL (used by availability, pricing, extensions and late fees):
//   * A rental period is [pickup date, return date): the return date is NOT a
//     billed day. 2026-11-12 -> 2026-11-15 is 3 billed days (12th, 13th, 14th).
//   * The return date must be after the pickup date (minimum 1 day).
//   * Back-to-back bookings are allowed: one booking may return on the same
//     date another booking for that vehicle is picked up.
//   * This version bills whole days only. There are no hours anywhere.
//
// Dates are stored as a count of days since 1970-01-01 using Howard Hinnant's
// civil-calendar algorithm, so leap years and month lengths are exact (no
// "month = 30 days" shortcuts).

#include <iosfwd>
#include <string>
#include <vector>

namespace rental {

class Date {
public:
    // 1970-01-01. Exists so Date can be used in containers; real dates come
    // from fromYmd() or parse().
    Date() = default;

    static Date fromYmd(int year, int month, int day);   // throws ValidationError
    static Date parse(const std::string& text);          // strict YYYY-MM-DD
    static Date fromDaysSinceEpoch(long long days) { return Date(days); }

    static bool isLeapYear(int year);
    static int daysInMonth(int year, int month);
    static bool isValid(int year, int month, int day);

    int year() const;
    int month() const;
    int day() const;

    // 0 = Sunday, 1 = Monday, ... 6 = Saturday
    int weekdayIndex() const;
    std::string weekdayName() const; // "Mon", "Tue", ...
    bool isWeekend() const;          // Saturday or Sunday

    Date addDays(long long n) const { return Date(days_ + n); }
    long long daysSinceEpoch() const { return days_; }

    // Signed number of days from `from` to `to` (to - from).
    static long long daysBetween(Date from, Date to) { return to.days_ - from.days_; }

    std::string toString() const; // YYYY-MM-DD

    bool operator==(Date o) const { return days_ == o.days_; }
    bool operator!=(Date o) const { return days_ != o.days_; }
    bool operator<(Date o) const { return days_ < o.days_; }
    bool operator<=(Date o) const { return days_ <= o.days_; }
    bool operator>(Date o) const { return days_ > o.days_; }
    bool operator>=(Date o) const { return days_ >= o.days_; }

private:
    explicit Date(long long days) : days_(days) {}
    long long days_ = 0;
};

std::ostream& operator<<(std::ostream& os, Date d);

// Half-open interval [start, end) of whole days. Always non-empty.
class DateRange {
public:
    DateRange(Date start, Date end); // throws ValidationError unless end > start

    Date start() const { return start_; }
    Date end() const { return end_; }        // exclusive
    long long lengthDays() const { return Date::daysBetween(start_, end_); }

    bool contains(Date d) const { return start_ <= d && d < end_; }
    // True when the two periods share at least one billed day.
    // [1,5) and [5,8) do NOT overlap (back-to-back).
    bool overlaps(const DateRange& other) const {
        return start_ < other.end_ && other.start_ < end_;
    }
    std::vector<Date> days() const; // every billed day, in order

    std::string toString() const; // "2026-11-12 -> 2026-11-15 (3 days)"

    bool operator==(const DateRange& o) const { return start_ == o.start_ && end_ == o.end_; }
    bool operator!=(const DateRange& o) const { return !(*this == o); }

private:
    Date start_;
    Date end_;
};

} // namespace rental
