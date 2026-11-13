#include "rental/Booking.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

namespace rental {

Booking::Booking(std::string id, std::string customerId, std::string vehicleId, PriceBreakdown agreedPrice,
                 std::int64_t createdAtEpoch)
    : id_(requireText(id, "Booking ID")),
      customerId_(requireText(customerId, "Customer ID")),
      vehicleId_(requireText(vehicleId, "Vehicle ID")),
      createdAt_(createdAtEpoch),
      period_(agreedPrice.period()) {
    parts_.push_back(std::move(agreedPrice));
}

Money Booking::agreedRentalTotal() const {
    Money sum;
    for (const PriceBreakdown& p : parts_) sum += p.total();
    return sum;
}

void Booking::requireStatus(BookingStatus expected, const std::string& action) const {
    if (status_ != expected) {
        throw ConflictError("Cannot " + action + " booking " + id_ + ": it is " + toString(status_) + " (must be " +
                            toString(expected) + ").");
    }
}

void Booking::cancel() {
    requireStatus(BookingStatus::Confirmed, "cancel");
    status_ = BookingStatus::Cancelled;
}

void Booking::addExtension(const PriceBreakdown& extension) {
    if (!occupiesVehicle()) {
        throw ConflictError("Cannot extend booking " + id_ + ": it is " + toString(status_) + ".");
    }
    if (extension.period().start() != period_.end()) {
        throw ConflictError("Extension must start on the current return date " + period_.end().toString() + ".");
    }
    period_ = DateRange(period_.start(), extension.period().end());
    parts_.push_back(extension);
}

void Booking::recordPickup(const InspectionReport& report, Money deposit) {
    requireStatus(BookingStatus::Confirmed, "collect");
    if (!period_.contains(report.date())) {
        throw ValidationError("Pickup date " + report.date().toString() + " is outside the booking period " +
                              period_.toString() + ".");
    }
    if (deposit < Money()) throw ValidationError("Deposit cannot be negative.");
    pickup_ = report;
    deposit_ = deposit;
    status_ = BookingStatus::Active;
}

void Booking::validateReturn(const InspectionReport& report) const {
    requireStatus(BookingStatus::Active, "return");
    if (report.date() < pickup_->date()) {
        throw ValidationError("Return date " + report.date().toString() + " is before the pickup date " +
                              pickup_->date().toString() + ".");
    }
    if (report.odometerKm() < pickup_->odometerKm()) {
        throw ValidationError("Return odometer " + std::to_string(report.odometerKm()) +
                              " km is lower than the pickup reading " + std::to_string(pickup_->odometerKm()) + " km.");
    }
}

void Booking::recordReturn(const InspectionReport& report, const std::string& invoiceId) {
    validateReturn(report);
    return_ = report;
    invoiceId_ = requireText(invoiceId, "Invoice ID");
    status_ = BookingStatus::Completed;
}

} // namespace rental
