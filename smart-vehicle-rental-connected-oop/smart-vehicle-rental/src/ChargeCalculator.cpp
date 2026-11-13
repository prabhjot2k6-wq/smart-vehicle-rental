#include "rental/ChargeCalculator.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <algorithm>

namespace rental {

ChargeRules ChargeRules::defaults() {
    ChargeRules r;
    r.setLateDayMultiplierBp(15000);
    r.setForCategory(VehicleCategory::Car, {250, Money::fromRupees(10), Money::fromRupees(60)});
    r.setForCategory(VehicleCategory::Bike, {150, Money::fromRupees(4), Money::fromRupees(15)});
    r.setForCategory(VehicleCategory::Van, {200, Money::fromRupees(14), Money::fromRupees(90)});
    return r;
}

void ChargeRules::setLateDayMultiplierBp(BasisPoints bp) {
    if (bp < 0 || bp > 10 * kBpScale) throw ValidationError("Late-day multiplier must be 0.00x-10.00x.");
    lateDayMultiplierBp_ = bp;
}

const CategoryCharges& ChargeRules::forCategory(VehicleCategory category) const {
    auto it = byCategory_.find(category);
    if (it == byCategory_.end()) throw ValidationError("No charge rules for category " + toString(category) + ".");
    return it->second;
}

void ChargeRules::setForCategory(VehicleCategory category, const CategoryCharges& c) {
    if (c.freeKmPerDay < 0 || c.excessPerKm < Money() || c.fuelPerPercentPoint < Money()) {
        throw ValidationError("Charge rules cannot be negative.");
    }
    byCategory_[category] = c;
}

DamageCharge::DamageCharge(std::string r, Money a) : reason(trim(r)), amount(a) {
    if (reason.empty()) throw ValidationError("A damage charge needs a reason.");
    if (amount <= Money()) throw ValidationError("A damage charge must be greater than zero.");
}

std::vector<InvoiceLine> ChargeCalculator::rentalLines(const Booking& booking) const {
    std::vector<InvoiceLine> lines;
    const auto& parts = booking.priceParts();
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const PriceBreakdown& p = parts[i];
        const bool original = i == 0;
        lines.push_back({original ? ChargeType::Rental : ChargeType::Extension,
                         std::string(original ? "Rental " : "Extension ") + p.period().toString() + " [" +
                             p.strategyName() + " pricing]",
                         p.subtotal()});
        if (p.discountBp() > 0) {
            lines.push_back({ChargeType::DurationDiscount,
                             "Duration discount " + formatPercent(p.discountBp()) + " on " +
                                 (original ? std::string("rental") : "extension " + std::to_string(i)),
                             -p.discount()});
        }
    }
    return lines;
}

std::vector<InvoiceLine> ChargeCalculator::returnCharges(const Booking& booking, VehicleCategory category,
                                                         const InspectionReport& ret,
                                                         const std::vector<DamageCharge>& damages) const {
    if (!booking.pickupReport()) throw ConflictError("Booking " + booking.id() + " has no pickup record.");
    const InspectionReport& pick = *booking.pickupReport();
    if (ret.odometerKm() < pick.odometerKm()) {
        throw ValidationError("Return odometer cannot be lower than the pickup odometer.");
    }
    const CategoryCharges& c = rules_.forCategory(category);
    std::vector<InvoiceLine> lines;

    // Late return: whole days after the agreed (exclusive) return date.
    const long long lateDays = std::max(0LL, Date::daysBetween(booking.period().end(), ret.date()));
    if (lateDays > 0 && rules_.lateDayMultiplierBp() > 0) {
        const Money perDay = booking.priceParts().front().baseDailyRate().applyBasisPoints(rules_.lateDayMultiplierBp());
        lines.push_back({ChargeType::LateReturn,
                         "Late return: " + std::to_string(lateDays) + " day(s) after " +
                             booking.period().end().toString() + " x " + perDay.toString() + " (" +
                             formatFactor(rules_.lateDayMultiplierBp()) + " base rate)",
                         perDay * lateDays});
    }

    // Excess mileage: allowance covers booked days plus any late days.
    const long long driven = ret.odometerKm() - pick.odometerKm();
    const long long allowance = c.freeKmPerDay * (booking.period().lengthDays() + lateDays);
    if (driven > allowance) {
        const long long extra = driven - allowance;
        lines.push_back({ChargeType::ExcessMileage,
                         "Excess mileage: " + std::to_string(driven) + " km driven, " + std::to_string(allowance) +
                             " km included, " + std::to_string(extra) + " km x " + c.excessPerKm.toString(),
                         c.excessPerKm * extra});
    }

    // Fuel: charge for each percentage point below the pickup level.
    if (ret.fuelPercent() < pick.fuelPercent()) {
        const int missing = pick.fuelPercent() - ret.fuelPercent();
        lines.push_back({ChargeType::Fuel,
                         "Fuel/charge shortfall: " + std::to_string(pick.fuelPercent()) + "% -> " +
                             std::to_string(ret.fuelPercent()) + "% (" + std::to_string(missing) + " points x " +
                             c.fuelPerPercentPoint.toString() + ")",
                         c.fuelPerPercentPoint * missing});
    }

    for (const DamageCharge& d : damages) {
        lines.push_back({ChargeType::Damage, "Damage: " + d.reason, d.amount});
    }
    return lines;
}

} // namespace rental
