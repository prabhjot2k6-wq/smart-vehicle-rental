#pragma once
// Pricing strategies (Strategy design pattern).
//
// PricingStrategy is an INTERFACE (only pure virtual functions). QuoteService
// holds a std::unique_ptr<PricingStrategy> and calls price() without knowing
// which concrete strategy it has - runtime polymorphism. Swapping the strategy
// changes how quotes are priced without touching any other class.
//
// PricingContext is a SNAPSHOT: it copies the base rate, the rules and the
// per-day demand at the moment the quote is generated. The strategy only reads
// the snapshot, so the whole quote is calculated from one consistent view.

#include "rental/PriceBreakdown.h"
#include "rental/PricingRules.h"

#include <string>
#include <vector>

namespace rental {

struct DemandDay {
    Date date;
    long long booked;   // confirmed + active reservations, excluding the proposed one
    long long capacity; // rentable (Active) vehicles of the model
};

class PricingContext {
public:
    PricingContext(std::string modelName, Money baseDailyRate, DateRange period, PricingRules rules,
                   std::vector<DemandDay> demand);

    const std::string& modelName() const { return modelName_; }
    Money baseDailyRate() const { return baseDailyRate_; }
    const DateRange& period() const { return period_; }
    const PricingRules& rules() const { return rules_; }
    const std::vector<DemandDay>& demand() const { return demand_; }

private:
    std::string modelName_;
    Money baseDailyRate_;
    DateRange period_;
    PricingRules rules_;
    std::vector<DemandDay> demand_;
};

class PricingStrategy {
public:
    virtual ~PricingStrategy() = default;
    virtual std::string name() const = 0;
    virtual PriceBreakdown price(const PricingContext& context) const = 0;
};

// Flat base rate every day; only the duration discount applies.
class StandardPricingStrategy final : public PricingStrategy {
public:
    std::string name() const override { return "Standard"; }
    PriceBreakdown price(const PricingContext& context) const override;
};

// Base rate x min(demand factor x calendar factor, cap), then duration discount.
class DynamicPricingStrategy final : public PricingStrategy {
public:
    std::string name() const override { return "Dynamic"; }
    PriceBreakdown price(const PricingContext& context) const override;
};

} // namespace rental
