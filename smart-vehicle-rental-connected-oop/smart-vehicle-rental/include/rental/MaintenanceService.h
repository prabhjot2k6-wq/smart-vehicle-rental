#pragma once
// MaintenanceService: opening/closing maintenance and service warnings.
//
// WARNING vs BLOCK (a common viva question):
//  * A service WARNING means the odometer is close to (or past) the next
//    service threshold. It is shown in reports, but the vehicle can still be
//    booked - the business decides when to take it off the road.
//  * A BLOCK happens only when a maintenance record is OPEN: the vehicle's
//    status becomes UnderMaintenance, it leaves the rentable capacity and
//    cannot be quoted, booked or collected until the record is completed.

#include "rental/AvailabilityService.h"
#include "rental/MaintenanceRecord.h"
#include "rental/RentalData.h"

#include <string>
#include <vector>

namespace rental {

struct ServiceAlert {
    const Vehicle* vehicle;
    long long kmRemaining; // negative = overdue
};

class MaintenanceService {
public:
    MaintenanceService(RentalData& data, const AvailabilityService& availability)
        : data_(data), availability_(availability) {}

    // Blocks the vehicle. Refused if it is currently with a customer.
    const MaintenanceRecord& openMaintenance(const std::string& vehicleId, const std::string& description,
                                             Date openedOn);
    // Unblocks the vehicle and schedules the next service from the new odometer.
    const MaintenanceRecord& completeMaintenance(const std::string& recordId, Date closedOn,
                                                 long long serviceOdometerKm, Money cost);

    // Confirmed bookings that will fail at pickup unless maintenance finishes first.
    std::vector<const Booking*> bookingsAffectedBy(const std::string& vehicleId) const;
    // Vehicles within `serviceWarningKm` of their next service (or overdue). Warning only.
    std::vector<ServiceAlert> serviceAlerts() const;
    std::vector<const MaintenanceRecord*> records() const { return data_.maintenance.all(); }

private:
    RentalData& data_;
    const AvailabilityService& availability_;
};

} // namespace rental
