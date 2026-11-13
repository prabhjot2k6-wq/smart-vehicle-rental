#pragma once
// Synthetic demonstration data. Used ONLY by `rental_app --seed-demo`, which
// refuses to run if a data file already exists - so normal restarts can never
// duplicate demo records.
//
// The data is created through the real services (so it obeys every rule),
// using a ManualClock that is moved through the past to build a realistic
// history: completed rentals with invoices, an active rental, future bookings
// that create demand, a cancelled booking and a vehicle under maintenance.

#include "rental/Clock.h"
#include "rental/RentalSystem.h"

namespace rental {

// `system` must have been constructed with `clock`. All dates are relative to `today`.
void populateDemoData(RentalSystem& system, ManualClock& clock, Date today);

} // namespace rental
