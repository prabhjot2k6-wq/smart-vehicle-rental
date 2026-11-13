#pragma once
// Read-only reports. ReportService never changes data.

#include "rental/AvailabilityService.h"
#include "rental/RentalData.h"

#include <optional>
#include <string>
#include <vector>

namespace rental {

struct FleetStatusRow {
    const Vehicle* vehicle;
    const VehicleModel* model;
    std::string state; // Available / Reserved (B-..) / On rent (B-..) / Under maintenance / Inactive
};

struct ActiveRentalRow {
    const Booking* booking;
    bool overdue;        // today is after the agreed return date
    long long daysLate;  // 0 unless overdue
};

struct RevenueSummary {
    int invoiceCount = 0;
    Money rentalCharges;     // rental + extensions - discounts
    Money additionalCharges; // late, mileage, fuel, damage
    Money total;
};

class ReportService {
public:
    ReportService(const RentalData& data, const AvailabilityService& availability)
        : data_(data), availability_(availability) {}

    std::vector<FleetStatusRow> fleetAvailability(Date day) const;
    std::vector<ActiveRentalRow> activeRentals(Date today) const;
    // Finalized invoices only (deposits excluded). `period` filters by issue
    // date using [start, end); no period = all time.
    RevenueSummary revenue(const std::optional<DateRange>& period) const;

private:
    const RentalData& data_;
    const AvailabilityService& availability_;
};

} // namespace rental
