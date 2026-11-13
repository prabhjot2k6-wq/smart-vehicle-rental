#pragma once
// Return charges: late return, excess mileage, fuel shortfall and damage.
//
// Rules are configurable (ChargeRules). Damage is NEVER inferred from the
// inspection notes: staff enter each damage charge with a reason and amount.

#include "rental/Booking.h"
#include "rental/Invoice.h"

#include <map>
#include <string>
#include <vector>

namespace rental {

struct CategoryCharges {
    long long freeKmPerDay;    // km included per billed day
    Money excessPerKm;         // charged for each km above the allowance
    Money fuelPerPercentPoint; // charged per % of tank missing on return
};

class ChargeRules {
public:
    static ChargeRules defaults();

    // Late fee per day = original base daily rate x this multiplier.
    BasisPoints lateDayMultiplierBp() const { return lateDayMultiplierBp_; }
    void setLateDayMultiplierBp(BasisPoints bp);

    const CategoryCharges& forCategory(VehicleCategory category) const;
    void setForCategory(VehicleCategory category, const CategoryCharges& charges);

private:
    BasisPoints lateDayMultiplierBp_ = 15000;
    std::map<VehicleCategory, CategoryCharges> byCategory_;
};

struct DamageCharge {
    DamageCharge(std::string reason, Money amount); // validated
    std::string reason;
    Money amount;
};

class ChargeCalculator {
public:
    explicit ChargeCalculator(const ChargeRules& rules) : rules_(rules) {}

    // Lines for the agreed rental price: original period, extensions, discounts.
    std::vector<InvoiceLine> rentalLines(const Booking& booking) const;

    // Extra charges at return. `booking` must have a pickup report.
    std::vector<InvoiceLine> returnCharges(const Booking& booking, VehicleCategory category,
                                           const InspectionReport& returnReport,
                                           const std::vector<DamageCharge>& damages) const;

private:
    const ChargeRules& rules_;
};

} // namespace rental
