#pragma once
// RentalData: every repository plus configuration, in one movable bundle.
//
// It is a plain data holder (no business rules). Services receive a reference
// to it; DataStore saves and loads it as a whole.

#include "rental/Booking.h"
#include "rental/ChargeCalculator.h"
#include "rental/Customer.h"
#include "rental/Invoice.h"
#include "rental/MaintenanceRecord.h"
#include "rental/PricingRules.h"
#include "rental/Repository.h"
#include "rental/VehicleModel.h"

namespace rental {

struct RentalData {
    Repository<VehicleModel> models{"Vehicle model"};
    VehicleRepository vehicles;
    Repository<Customer> customers{"Customer"};
    Repository<Booking> bookings{"Booking"};
    Repository<Invoice> invoices{"Invoice"};
    Repository<MaintenanceRecord> maintenance{"Maintenance record"};
    IdGenerator ids;
    PricingRules pricingRules = PricingRules::defaults();
    ChargeRules chargeRules = ChargeRules::defaults();
    long long serviceWarningKm = 500; // warn when this close to the next service
};

} // namespace rental
