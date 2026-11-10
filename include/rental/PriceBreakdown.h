#pragma once
// The result of pricing a period: one line per day plus totals.
// Stored inside each Booking, so later rule/rate changes never alter a
// confirmed price (the booking keeps its own copy).

#include "rental/Date.h"
#include "rental/Money.h"

#include <string>
#include <vector>

namespace rental {

struct DailyPrice {
    Date date;
    long long booked = 0;    // existing reservations for the model that day
    long long capacity = 0;  // rentable vehicles of the model that day
    BasisPoints demandFactorBp = kBpScale;
    std::string demandReason;
    BasisPoints calendarFactorBp = kBpScale;
    std::string calendarReason;
    BasisPoints appliedFactorBp = kBpScale; // demand x calendar, after the cap
    bool capped = false;
    Money amount;
};

class PriceBreakdown {
public:
    PriceBreakdown(DateRange period, std::string strategyName, Money baseDailyRate)
        : period_(period), strategyName_(std::move(strategyName)), baseDailyRate_(baseDailyRate) {}

    const DateRange& period() const { return period_; }
    const std::string& strategyName() const { return strategyName_; }
    Money baseDailyRate() const { return baseDailyRate_; }
    const std::vector<DailyPrice>& days() const { return days_; }
    Money subtotal() const { return subtotal_; }
    BasisPoints discountBp() const { return discountBp_; }
    Money discount() const { return discount_; }
    Money total() const { return subtotal_ - discount_; }

    void addDay(const DailyPrice& day) {
        days_.push_back(day);
        subtotal_ += day.amount;
    }
    // discount = subtotal x discountBp, rounded half away from zero.
    void applyDurationDiscount(BasisPoints discountBp) {
        discountBp_ = discountBp;
        discount_ = subtotal_.applyBasisPoints(discountBp);
    }

private:
    DateRange period_;
    std::string strategyName_;
    Money baseDailyRate_;
    std::vector<DailyPrice> days_;
    Money subtotal_;
    BasisPoints discountBp_ = 0;
    Money discount_;
};

} // namespace rental
