#include "rental/PricingStrategy.h"

#include "rental/Errors.h"

#include <algorithm>

namespace rental {

PricingContext::PricingContext(std::string modelName, Money baseDailyRate, DateRange period, PricingRules rules,
                               std::vector<DemandDay> demand)
    : modelName_(std::move(modelName)),
      baseDailyRate_(baseDailyRate),
      period_(period),
      rules_(std::move(rules)),
      demand_(std::move(demand)) {
    if (baseDailyRate_ <= Money()) throw ValidationError("Base daily rate must be positive.");
    const std::vector<Date> days = period_.days();
    if (demand_.size() != days.size()) throw ValidationError("Demand snapshot must have one entry per rental day.");
    for (std::size_t i = 0; i < days.size(); ++i) {
        if (demand_[i].date != days[i]) throw ValidationError("Demand snapshot dates do not match the period.");
        if (demand_[i].booked < 0 || demand_[i].capacity < 0) throw ValidationError("Demand counts cannot be negative.");
    }
}

namespace {
void finish(PriceBreakdown& b, const PricingContext& ctx) {
    b.applyDurationDiscount(ctx.rules().durationDiscountBp(ctx.period().lengthDays()));
}
} // namespace

PriceBreakdown StandardPricingStrategy::price(const PricingContext& ctx) const {
    PriceBreakdown b(ctx.period(), name(), ctx.baseDailyRate());
    for (const DemandDay& d : ctx.demand()) {
        DailyPrice day;
        day.date = d.date;
        day.booked = d.booked;
        day.capacity = d.capacity;
        day.demandReason = "standard strategy ignores demand";
        day.calendarReason = "standard strategy ignores calendar";
        day.amount = ctx.baseDailyRate();
        b.addDay(day);
    }
    finish(b, ctx);
    return b;
}

PriceBreakdown DynamicPricingStrategy::price(const PricingContext& ctx) const {
    const PricingRules& rules = ctx.rules();
    PriceBreakdown b(ctx.period(), name(), ctx.baseDailyRate());
    for (const DemandDay& d : ctx.demand()) {
        DailyPrice day;
        day.date = d.date;
        day.booked = d.booked;
        day.capacity = d.capacity;

        const DemandDecision demand = rules.demandFactor(d.booked, d.capacity);
        day.demandFactorBp = demand.factorBp;
        day.demandReason = demand.reason;

        // Calendar factor: weekend OR holiday. If both, use the higher one (never multiply them).
        const bool weekend = d.date.isWeekend();
        const bool holiday = rules.isHoliday(d.date);
        if (weekend && holiday) {
            day.calendarFactorBp = std::max(rules.weekendFactorBp(), rules.holidayFactorBp());
            day.calendarReason = "weekend + holiday: higher factor used";
        } else if (holiday) {
            day.calendarFactorBp = rules.holidayFactorBp();
            day.calendarReason = "holiday";
        } else if (weekend) {
            day.calendarFactorBp = rules.weekendFactorBp();
            day.calendarReason = "weekend";
        } else {
            day.calendarFactorBp = kBpScale;
            day.calendarReason = "weekday";
        }

        const BasisPoints combined = multiplyBp(day.demandFactorBp, day.calendarFactorBp);
        day.capped = combined > rules.maxDailyFactorBp();
        day.appliedFactorBp = day.capped ? rules.maxDailyFactorBp() : combined;
        day.amount = ctx.baseDailyRate().applyBasisPoints(day.appliedFactorBp);
        b.addDay(day);
    }
    finish(b, ctx);
    return b;
}

} // namespace rental
