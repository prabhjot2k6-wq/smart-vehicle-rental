#include "rental/ReportService.h"

namespace rental {

std::vector<FleetStatusRow> ReportService::fleetAvailability(Date day) const {
    std::vector<FleetStatusRow> rows;
    for (const Vehicle* v : data_.vehicles.all()) {
        std::string state = "Available";
        if (v->status() == VehicleStatus::Inactive) {
            state = "Inactive";
        } else if (v->status() == VehicleStatus::UnderMaintenance) {
            state = "Under maintenance (blocked)";
        } else {
            for (const Booking* b : availability_.openBookingsFor(v->id())) {
                const bool active = b->status() == BookingStatus::Active;
                // An overdue active rental still holds the vehicle after its end date.
                if (b->period().contains(day) || (active && day >= b->period().end())) {
                    state = (active ? "On rent (" : "Reserved (") + b->id() + ")";
                    break;
                }
            }
        }
        rows.push_back({v, &data_.models.get(v->modelId()), state});
    }
    return rows;
}

std::vector<ActiveRentalRow> ReportService::activeRentals(Date today) const {
    std::vector<ActiveRentalRow> rows;
    for (const Booking* b : data_.bookings.all()) {
        if (b->status() != BookingStatus::Active) continue;
        const long long late = Date::daysBetween(b->period().end(), today);
        rows.push_back({b, late > 0, late > 0 ? late : 0});
    }
    return rows;
}

RevenueSummary ReportService::revenue(const std::optional<DateRange>& period) const {
    RevenueSummary s;
    for (const Invoice* inv : data_.invoices.all()) {
        if (inv->status() != InvoiceStatus::Finalized) continue;
        if (period && !period->contains(inv->issueDate())) continue;
        ++s.invoiceCount;
        s.rentalCharges += inv->rentalCharges();
        s.additionalCharges += inv->additionalCharges();
        s.total += inv->totalCharges();
    }
    return s;
}

} // namespace rental
