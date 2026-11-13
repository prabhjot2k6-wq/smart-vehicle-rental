#include "rental/PricingRules.h"

#include "rental/Errors.h"

namespace rental {

namespace {
void requireFactor(BasisPoints bp, const char* what) {
    if (bp <= 0 || bp > 5 * kBpScale) {
        throw ValidationError(std::string(what) + " must be greater than 0.00x and at most 5.00x.");
    }
}
} // namespace

PricingRules PricingRules::defaults() {
    PricingRules r;
    r.setDemandTiers({{0, 9000}, {3000, 10000}, {7000, 11500}, {9000, 12500}});
    r.setWeekendFactorBp(11000);
    r.setHolidayFactorBp(11500);
    r.setMaxDailyFactorBp(14000);
    r.setDurationDiscounts({{7, 1000}, {30, 2000}});
    r.setQuoteValidityMinutes(15);
    return r;
}

void PricingRules::setDemandTiers(std::vector<DemandTier> tiers) {
    if (tiers.empty() || tiers.front().minUtilizationBp != 0) {
        throw ValidationError("Demand tiers must start at 0% utilization.");
    }
    for (std::size_t i = 0; i < tiers.size(); ++i) {
        requireFactor(tiers[i].factorBp, "Demand factor");
        if (tiers[i].minUtilizationBp < 0 || tiers[i].minUtilizationBp > kBpScale) {
            throw ValidationError("Demand tier thresholds must be between 0% and 100%.");
        }
        if (i > 0 && tiers[i].minUtilizationBp <= tiers[i - 1].minUtilizationBp) {
            throw ValidationError("Demand tier thresholds must be strictly increasing.");
        }
    }
    demandTiers_ = std::move(tiers);
}

void PricingRules::setWeekendFactorBp(BasisPoints bp) {
    requireFactor(bp, "Weekend factor");
    weekendFactorBp_ = bp;
}

void PricingRules::setHolidayFactorBp(BasisPoints bp) {
    requireFactor(bp, "Holiday factor");
    holidayFactorBp_ = bp;
}

void PricingRules::setMaxDailyFactorBp(BasisPoints bp) {
    requireFactor(bp, "Daily factor cap");
    maxDailyFactorBp_ = bp;
}

void PricingRules::setDurationDiscounts(std::vector<DurationDiscount> discounts) {
    for (std::size_t i = 0; i < discounts.size(); ++i) {
        if (discounts[i].minDays < 1) throw ValidationError("Discount minimum days must be at least 1.");
        if (discounts[i].discountBp <= 0 || discounts[i].discountBp >= kBpScale) {
            throw ValidationError("Duration discount must be between 0% and 100% (exclusive).");
        }
        if (i > 0 && discounts[i].minDays <= discounts[i - 1].minDays) {
            throw ValidationError("Duration discounts must be sorted by increasing minimum days.");
        }
    }
    durationDiscounts_ = std::move(discounts);
}

void PricingRules::setQuoteValidityMinutes(int minutes) {
    if (minutes < 1 || minutes > 24 * 60) throw ValidationError("Quote validity must be 1-1440 minutes.");
    quoteValidityMinutes_ = minutes;
}

DemandDecision PricingRules::demandFactor(long long booked, long long capacity) const {
    if (capacity <= 0) {
        return {kBpScale, "no rentable vehicles of this model: neutral demand"};
    }
    const DemandTier* chosen = &demandTiers_.front();
    for (const DemandTier& tier : demandTiers_) {
        if (booked * kBpScale >= tier.minUtilizationBp * capacity) chosen = &tier;
    }
    // Utilization shown to one decimal place, for the explanation only.
    const long long tenthsOfPercent = divideRounded(booked * 1000, capacity);
    std::string pct = std::to_string(tenthsOfPercent / 10) + "." + std::to_string(tenthsOfPercent % 10) + "%";
    return {chosen->factorBp, std::to_string(booked) + "/" + std::to_string(capacity) + " booked (" + pct +
                                  ") -> tier >= " + formatPercent(chosen->minUtilizationBp)};
}

BasisPoints PricingRules::durationDiscountBp(long long rentalDays) const {
    BasisPoints best = 0;
    for (const DurationDiscount& d : durationDiscounts_) {
        if (rentalDays >= d.minDays && d.discountBp > best) best = d.discountBp;
    }
    return best;
}

} // namespace rental
