#pragma once
// RentalSystem is the "composition root": it owns the data and creates each
// service, wiring them together. It holds no business rules itself - each
// rule lives in the one service responsible for it.

#include "rental/CustomerService.h"
#include "rental/FleetService.h"
#include "rental/RentalData.h"

namespace rental {

class RentalSystem {
public:
    explicit RentalSystem(RentalData data);

    // Services keep references into data_, so the system must not move.
    RentalSystem(const RentalSystem&) = delete;
    RentalSystem& operator=(const RentalSystem&) = delete;

    RentalData& data() { return data_; }
    const RentalData& data() const { return data_; }
    FleetService& fleet() { return fleet_; }
    CustomerService& customers() { return customers_; }

private:
    RentalData data_; // declared first: constructed before the services that use it
    FleetService fleet_;
    CustomerService customers_;
};

} // namespace rental
