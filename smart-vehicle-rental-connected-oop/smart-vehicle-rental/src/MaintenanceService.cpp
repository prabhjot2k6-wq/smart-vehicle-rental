#include "rental/MaintenanceService.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <algorithm>

namespace rental {

MaintenanceRecord::MaintenanceRecord(std::string id, std::string vehicleId, std::string description, Date openedOn,
                                     long long odometerAtOpenKm)
    : id_(requireText(id, "Maintenance ID")),
      vehicleId_(requireText(vehicleId, "Vehicle ID")),
      description_(requireText(description, "Maintenance description")),
      openedOn_(openedOn),
      odometerAtOpenKm_(odometerAtOpenKm) {
    if (odometerAtOpenKm_ < 0) throw ValidationError("Odometer cannot be negative.");
}

void MaintenanceRecord::complete(Date closedOn, long long serviceOdometerKm, Money cost) {
    if (status_ != MaintenanceStatus::Open) throw ConflictError("Maintenance " + id_ + " is already completed.");
    if (closedOn < openedOn_) throw ValidationError("Completion date cannot be before the opening date.");
    if (serviceOdometerKm < odometerAtOpenKm_) {
        throw ValidationError("Service odometer cannot be lower than the reading when maintenance began.");
    }
    if (cost < Money()) throw ValidationError("Maintenance cost cannot be negative.");
    closedOn_ = closedOn;
    serviceOdometerKm_ = serviceOdometerKm;
    cost_ = cost;
    status_ = MaintenanceStatus::Completed;
}

const MaintenanceRecord& MaintenanceService::openMaintenance(const std::string& vehicleId,
                                                             const std::string& description, Date openedOn) {
    Vehicle& v = data_.vehicles.get(vehicleId);
    if (const Booking* rental = availability_.activeRentalFor(vehicleId)) {
        throw ConflictError("Vehicle " + vehicleId + " is currently out on rental " + rental->id() +
                            ". Record the return first.");
    }
    MaintenanceRecord probe("pending", vehicleId, description, openedOn, v.mileageKm()); // validate first
    v.startMaintenance(); // throws if not Active (inactive or already under maintenance)
    return data_.maintenance.add(
        MaintenanceRecord(data_.ids.next("MT"), vehicleId, probe.description(), openedOn, v.mileageKm()));
}

const MaintenanceRecord& MaintenanceService::completeMaintenance(const std::string& recordId, Date closedOn,
                                                                 long long serviceOdometerKm, Money cost) {
    MaintenanceRecord& r = data_.maintenance.get(recordId);
    Vehicle& v = data_.vehicles.get(r.vehicleId());
    if (serviceOdometerKm < v.mileageKm()) {
        throw ValidationError("Service odometer is lower than the vehicle's recorded " + std::to_string(v.mileageKm()) +
                              " km.");
    }
    r.complete(closedOn, serviceOdometerKm, cost);
    v.recordOdometer(serviceOdometerKm);
    v.setNextServiceDueKm(serviceOdometerKm + v.serviceIntervalKm());
    v.finishMaintenance();
    return r;
}

std::vector<const Booking*> MaintenanceService::bookingsAffectedBy(const std::string& vehicleId) const {
    std::vector<const Booking*> out;
    for (const Booking* b : availability_.openBookingsFor(vehicleId)) {
        if (b->status() == BookingStatus::Confirmed) out.push_back(b);
    }
    return out;
}

std::vector<ServiceAlert> MaintenanceService::serviceAlerts() const {
    std::vector<ServiceAlert> alerts;
    for (const Vehicle* v : data_.vehicles.all()) {
        if (v->status() == VehicleStatus::Inactive) continue;
        if (v->kmUntilService() <= data_.serviceWarningKm) alerts.push_back({v, v->kmUntilService()});
    }
    std::sort(alerts.begin(), alerts.end(),
              [](const ServiceAlert& a, const ServiceAlert& b) { return a.kmRemaining < b.kmRemaining; });
    return alerts;
}

} // namespace rental
