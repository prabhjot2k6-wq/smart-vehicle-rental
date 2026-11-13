#pragma once
// Configurable pricing rules. All factors and percentages are basis points
// (10000 = 1.00x = 100%). Every setter validates, so the rules can never be
// put into a nonsensical state (e.g. a negative weekend factor).

#include "rental/Date.h"
#include "rental/Money.h"

#include <set>
#include <string>
#include <vector>

namespace rental {

// Applies when utilization >= minUtilizationBp (e.g. 7000 = 70%).
struct DemandTier {
    BasisPoints minUtilizationBp;
    BasisPoints factorBp;
};

// Applies when the rental lasts at least minDays.
struct DurationDiscount {
    long long minDays;
    BasisPoints discountBp;
};

struct DemandDecision {
    BasisPoints factorBp;
    std::string reason;
};

class PricingRules {
public:
    // The project's initial rules (see README "Pricing rules").
    static PricingRules defaults();

    const std::vector<DemandTier>& demandTiers() const { return demandTiers_; }
    void setDemandTiers(std::vector<DemandTier> tiers);

    BasisPoints weekendFactorBp() const { return weekendFactorBp_; }
    void setWeekendFactorBp(BasisPoints bp);
    BasisPoints holidayFactorBp() const { return holidayFactorBp_; }
    void setHolidayFactorBp(BasisPoints bp);
    BasisPoints maxDailyFactorBp() const { return maxDailyFactorBp_; }
    void setMaxDailyFactorBp(BasisPoints bp);

    const std::vector<DurationDiscount>& durationDiscounts() const { return durationDiscounts_; }
    void setDurationDiscounts(std::vector<DurationDiscount> discounts);

    int quoteValidityMinutes() const { return quoteValidityMinutes_; }
    void setQuoteValidityMinutes(int minutes);

    const std::set<Date>& holidays() const { return holidays_; }
    void addHoliday(Date d) { holidays_.insert(d); }
    bool removeHoliday(Date d) { return holidays_.erase(d) > 0; }
    bool isHoliday(Date d) const { return holidays_.count(d) > 0; }

    // Picks the demand tier for booked/capacity using exact integer
    // comparison (booked * 10000 >= threshold * capacity), so 7/10 is
    // exactly 70%. Zero capacity returns a neutral 1.00x factor.
    DemandDecision demandFactor(long long booked, long long capacity) const;

    // Highest eligible discount only (never stacked). 0 if none applies.
    BasisPoints durationDiscountBp(long long rentalDays) const;

private:
    std::vector<DemandTier> demandTiers_;
    BasisPoints weekendFactorBp_ = kBpScale;
    BasisPoints holidayFactorBp_ = kBpScale;
    BasisPoints maxDailyFactorBp_ = kBpScale;
    std::vector<DurationDiscount> durationDiscounts_;
    int quoteValidityMinutes_ = 15;
    std::set<Date> holidays_;
};

} // namespace rental
