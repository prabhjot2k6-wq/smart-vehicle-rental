#pragma once
// AvailabilityService answers two questions, always with [start, end) dates:
//   1. Is this vehicle free for this period? (no overlapping Confirmed or
//      Active booking, and the vehicle itself is Active)
//   2. How busy is this model on each day? (demand snapshot for pricing)
//
// Both accept an optional booking ID to ignore - used when extending a
// booking, so it does not clash with itself.

#include "rental/RentalData.h"
#include "rental/PricingStrategy.h"

#include <string>
#include <vector>

namespace rental {

class AvailabilityService {
public:
    explicit AvailabilityService(const RentalData& data) : data_(data) {}

    std::vector<const Booking*> conflictingBookings(const std::string& vehicleId, const DateRange& period,
                                                    const std::string& ignoreBookingId = "") const;

    bool isAvailable(const Vehicle& vehicle, const DateRange& period, const std::string& ignoreBookingId = "") const;

    // Throws ValidationError / ConflictError explaining why the vehicle cannot be booked.
    void requireAvailable(const Vehicle& vehicle, const DateRange& period,
                          const std::string& ignoreBookingId = "") const;

    // Per-day demand for a model.
    //   capacity = number of the model's vehicles that are Active (rentable)
    //   booked   = Confirmed/Active bookings on those vehicles covering that day
    // Cancelled/completed bookings, non-rentable vehicles and `ignoreBookingId`
    // are excluded. The customer's proposed booking does not exist yet, so it
    // is never counted.
    std::vector<DemandDay> modelDemand(const std::string& modelId, const DateRange& period,
                                       const std::string& ignoreBookingId = "") const;

    // The Active booking currently holding this vehicle, if any.
    const Booking* activeRentalFor(const std::string& vehicleId) const;
    // Confirmed + Active bookings of this vehicle.
    std::vector<const Booking*> openBookingsFor(const std::string& vehicleId) const;

private:
    const RentalData& data_;
};

} // namespace rental
