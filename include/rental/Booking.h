#pragma once
// A booking (reservation) of one vehicle by one customer.
//
// COMPOSITION: a Booking owns its price breakdowns (original + one per
// extension) and its pickup/return inspection reports. They live and die with
// the booking and are saved with it.
//
// State machine (each transition method checks the current status):
//
//   Confirmed --pickup--> Active --return--> Completed
//       |
//       +--cancel--> Cancelled
//
// Extensions are allowed while Confirmed or Active.

#include "rental/Enums.h"
#include "rental/InspectionReport.h"
#include "rental/PriceBreakdown.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rental {

class Booking {
public:
    Booking(std::string id, std::string customerId, std::string vehicleId, PriceBreakdown agreedPrice,
            std::int64_t createdAtEpoch);

    const std::string& id() const { return id_; }
    const std::string& customerId() const { return customerId_; }
    const std::string& vehicleId() const { return vehicleId_; }
    BookingStatus status() const { return status_; }
    std::int64_t createdAtEpoch() const { return createdAt_; }

    // Current period including extensions: [first pickup date, latest return date).
    const DateRange& period() const { return period_; }
    // [0] = original agreed price, [1..] = each extension, in order.
    const std::vector<PriceBreakdown>& priceParts() const { return parts_; }
    Money agreedRentalTotal() const;

    const std::optional<InspectionReport>& pickupReport() const { return pickup_; }
    const std::optional<InspectionReport>& returnReport() const { return return_; }
    Money depositHeld() const { return deposit_; }
    const std::string& invoiceId() const { return invoiceId_; }

    // Confirmed and Active bookings occupy their vehicle; Completed/Cancelled do not.
    bool occupiesVehicle() const { return status_ == BookingStatus::Confirmed || status_ == BookingStatus::Active; }

    // ---- state transitions ----
    void cancel();
    void addExtension(const PriceBreakdown& extension); // must start on the current return date
    void recordPickup(const InspectionReport& report, Money deposit);
    void validateReturn(const InspectionReport& report) const; // throws if the report is not acceptable
    void recordReturn(const InspectionReport& report, const std::string& invoiceId);

private:
    void requireStatus(BookingStatus expected, const std::string& action) const;

    std::string id_;
    std::string customerId_;
    std::string vehicleId_;
    BookingStatus status_ = BookingStatus::Confirmed;
    std::int64_t createdAt_;
    DateRange period_;
    std::vector<PriceBreakdown> parts_;
    std::optional<InspectionReport> pickup_;
    std::optional<InspectionReport> return_;
    Money deposit_;
    std::string invoiceId_;
};

} // namespace rental
