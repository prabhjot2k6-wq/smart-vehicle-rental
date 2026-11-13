#pragma once
// Clock abstraction: "what time / date is it now?"
//
// Services ask a Clock instead of calling std::time directly. The real app
// passes SystemClock; tests pass ManualClock so they can say "15 minutes
// later" without waiting. This is runtime polymorphism used for testability.

#include "rental/Date.h"

#include <cstdint>

namespace rental {

class Clock {
public:
    virtual ~Clock() = default;
    virtual std::int64_t nowEpochSeconds() const = 0; // for quote expiry
    virtual Date today() const = 0;                   // local calendar date
};

class SystemClock final : public Clock {
public:
    std::int64_t nowEpochSeconds() const override;
    Date today() const override;
};

class ManualClock final : public Clock {
public:
    explicit ManualClock(Date today) { setToday(today); }
    std::int64_t nowEpochSeconds() const override { return now_; }
    Date today() const override { return today_; }

    // Moves to 09:00 on the given date.
    void setToday(Date d) {
        today_ = d;
        now_ = d.daysSinceEpoch() * 86400 + 9 * 3600;
    }
    void advanceSeconds(std::int64_t seconds) { now_ += seconds; }

private:
    Date today_;
    std::int64_t now_ = 0;
};

} // namespace rental
