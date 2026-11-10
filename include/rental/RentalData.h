#pragma once
// RentalData: every repository plus configuration, in one movable bundle.
//
// It is a plain data holder (no business rules). Services receive a reference
// to it; DataStore saves and loads it as a whole.

#include "rental/Booking.h"
#include "rental/Customer.h"
#include "rental/PricingRules.h"
#include "rental/Repository.h"
#include "rental/VehicleModel.h"

namespace rental {

struct RentalData {
    Repository<VehicleModel> models{"Vehicle model"};
    VehicleRepository vehicles;
    Repository<Customer> customers{"Customer"};
    Repository<Booking> bookings{"Booking"};
    IdGenerator ids;
    PricingRules pricingRules = PricingRules::defaults();
};

} // namespace rental
