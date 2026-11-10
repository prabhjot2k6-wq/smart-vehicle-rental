#pragma once
// RentalSystem is the "composition root": it owns the data and creates each
// service, wiring them together. It holds no business rules itself - each
// rule lives in the one service responsible for it.

#include "rental/AvailabilityService.h"
#include "rental/Clock.h"
#include "rental/CustomerService.h"
#include "rental/FleetService.h"
#include "rental/MaintenanceService.h"
#include "rental/QuoteService.h"
#include "rental/RentalData.h"
#include "rental/RentalService.h"
#include "rental/ReportService.h"

namespace rental {

class RentalSystem {
public:
    // The clock must outlive the system (SystemClock in the app, ManualClock in tests).
    RentalSystem(RentalData data, const Clock& clock);

    // Services keep references into data_, so the system must not move or copy.
    RentalSystem(const RentalSystem&) = delete;
    RentalSystem& operator=(const RentalSystem&) = delete;

    RentalData& data() { return data_; }
    const RentalData& data() const { return data_; }
    const Clock& clock() const { return clock_; }

    FleetService& fleet() { return fleet_; }
    CustomerService& customers() { return customers_; }
    AvailabilityService& availability() { return availability_; }
    QuoteService& quotes() { return quotes_; }
    RentalService& rentals() { return rentals_; }
    MaintenanceService& maintenance() { return maintenance_; }
    const ReportService& reports() const { return reports_; }

private:
    // Declaration order = construction order: data first, then services.
    RentalData data_;
    const Clock& clock_;
    FleetService fleet_;
    CustomerService customers_;
    AvailabilityService availability_;
    QuoteService quotes_;
    ChargeCalculator charges_;
    RentalService rentals_;
    MaintenanceService maintenance_;
    ReportService reports_;
};

} // namespace rental
