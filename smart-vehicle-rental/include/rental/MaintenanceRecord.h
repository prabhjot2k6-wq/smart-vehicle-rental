#pragma once
// One maintenance task for one vehicle (e.g. "10,000 km service").
// While a record is Open the vehicle is UnderMaintenance (blocked).

#include "rental/Date.h"
#include "rental/Enums.h"
#include "rental/Money.h"

#include <optional>
#include <string>

namespace rental {

class MaintenanceRecord {
public:
    MaintenanceRecord(std::string id, std::string vehicleId, std::string description, Date openedOn,
                      long long odometerAtOpenKm);

    const std::string& id() const { return id_; }
    const std::string& vehicleId() const { return vehicleId_; }
    const std::string& description() const { return description_; }
    MaintenanceStatus status() const { return status_; }
    Date openedOn() const { return openedOn_; }
    long long odometerAtOpenKm() const { return odometerAtOpenKm_; }
    const std::optional<Date>& closedOn() const { return closedOn_; }
    long long serviceOdometerKm() const { return serviceOdometerKm_; }
    Money cost() const { return cost_; }

    void complete(Date closedOn, long long serviceOdometerKm, Money cost);

private:
    std::string id_;
    std::string vehicleId_;
    std::string description_;
    MaintenanceStatus status_ = MaintenanceStatus::Open;
    Date openedOn_;
    long long odometerAtOpenKm_;
    std::optional<Date> closedOn_;
    long long serviceOdometerKm_ = 0;
    Money cost_;
};

} // namespace rental
