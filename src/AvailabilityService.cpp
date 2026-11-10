#include "rental/AvailabilityService.h"

#include <set>

namespace rental {

std::vector<const Booking*> AvailabilityService::conflictingBookings(const std::string& vehicleId,
                                                                     const DateRange& period,
                                                                     const std::string& ignoreBookingId) const {
    std::vector<const Booking*> clashes;
    for (const Booking* b : data_.bookings.all()) {
        if (b->vehicleId() == vehicleId && b->id() != ignoreBookingId && b->occupiesVehicle() &&
            b->period().overlaps(period)) {
            clashes.push_back(b);
        }
    }
    return clashes;
}

bool AvailabilityService::isAvailable(const Vehicle& vehicle, const DateRange& period,
                                      const std::string& ignoreBookingId) const {
    return vehicle.isRentable() && conflictingBookings(vehicle.id(), period, ignoreBookingId).empty();
}

void AvailabilityService::requireAvailable(const Vehicle& vehicle, const DateRange& period,
                                           const std::string& ignoreBookingId) const {
    if (vehicle.status() == VehicleStatus::UnderMaintenance) {
        throw ConflictError("Vehicle " + vehicle.id() + " is under maintenance and cannot be booked.");
    }
    if (vehicle.status() == VehicleStatus::Inactive) {
        throw ConflictError("Vehicle " + vehicle.id() + " is inactive and cannot be booked.");
    }
    const auto clashes = conflictingBookings(vehicle.id(), period, ignoreBookingId);
    if (!clashes.empty()) {
        const Booking& b = *clashes.front();
        throw ConflictError("Vehicle " + vehicle.id() + " is already booked by " + b.id() + " for " +
                            b.period().toString() + ", which overlaps " + period.toString() + ".");
    }
}

std::vector<DemandDay> AvailabilityService::modelDemand(const std::string& modelId, const DateRange& period,
                                                        const std::string& ignoreBookingId) const {
    std::set<std::string> rentableIds;
    for (const Vehicle* v : data_.vehicles.all()) {
        if (v->modelId() == modelId && v->isRentable()) rentableIds.insert(v->id());
    }
    std::vector<const Booking*> relevant;
    for (const Booking* b : data_.bookings.all()) {
        if (b->occupiesVehicle() && b->id() != ignoreBookingId && rentableIds.count(b->vehicleId()) != 0 &&
            b->period().overlaps(period)) {
            relevant.push_back(b);
        }
    }
    std::vector<DemandDay> demand;
    for (Date day : period.days()) {
        long long booked = 0;
        for (const Booking* b : relevant) booked += b->period().contains(day) ? 1 : 0;
        demand.push_back({day, booked, static_cast<long long>(rentableIds.size())});
    }
    return demand;
}

const Booking* AvailabilityService::activeRentalFor(const std::string& vehicleId) const {
    for (const Booking* b : data_.bookings.all()) {
        if (b->vehicleId() == vehicleId && b->status() == BookingStatus::Active) return b;
    }
    return nullptr;
}

std::vector<const Booking*> AvailabilityService::openBookingsFor(const std::string& vehicleId) const {
    std::vector<const Booking*> out;
    for (const Booking* b : data_.bookings.all()) {
        if (b->vehicleId() == vehicleId && b->occupiesVehicle()) out.push_back(b);
    }
    return out;
}

} // namespace rental
