// Pickup, return charges, invoices and maintenance.

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace rental;
using namespace testhelp;

namespace {
InspectionReport Insp(const char* date, long long km, int fuel, const std::string& notes = "") {
    return InspectionReport(D(date), km, fuel, notes);
}
} // namespace

TEST_CASE("return charges: late, excess mileage, fuel and manual damage are itemised") {
    Fleet f;
    // Creta, Mon 16 -> Thu 19 (3 days), 0/2 booked -> 0.90x -> 3 x 2250 = 6750.
    const std::string b = f.book(f.cretas[0], R("2026-11-16", "2026-11-19")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10000, 100, "Clean, no scratches"));
    CHECK_EQ(f.sys.rentals().booking(b).depositHeld(), Money::fromRupees(5000));

    // Returned Fri 20th: 1 day late, 1200 km driven, fuel 100% -> 80%.
    const std::vector<DamageCharge> damage = {DamageCharge("Scratch on rear bumper", Money::fromRupees(1500))};
    const Invoice preview = f.sys.rentals().previewReturn(b, Insp("2026-11-20", 11200, 80), damage);
    CHECK_EQ(preview.status(), InvoiceStatus::Draft);
    CHECK_EQ(f.sys.data().invoices.size(), std::size_t{0}); // preview saves nothing

    const Invoice& inv = f.sys.rentals().completeReturn(b, Insp("2026-11-20", 11200, 80, "Scratch rear"), damage);
    CHECK_EQ(inv.status(), InvoiceStatus::Finalized);
    CHECK_EQ(inv.rentalCharges(), Money::fromRupees(6750));
    // Late: 2500 x 1.5 = 3750. Mileage: allowance 250 x (3 + 1 late) = 1000; 200 km x 10 = 2000.
    // Fuel: 20 points x 60 = 1200. Damage: 1500.
    CHECK_EQ(inv.additionalCharges(), Money::fromRupees(3750 + 2000 + 1200 + 1500));
    CHECK_EQ(inv.totalCharges(), Money::fromRupees(15200));
    CHECK_EQ(inv.lines().size(), std::size_t{5});
    CHECK_EQ(inv.depositRefunded(), Money::fromRupees(5000)); // separate from charges
    CHECK_EQ(f.sys.rentals().booking(b).status(), BookingStatus::Completed);
    CHECK_EQ(f.sys.fleet().vehicle(f.cretas[0]).mileageKm(), 11200LL);
    CHECK_THROWS_AS(f.sys.rentals().completeReturn(b, Insp("2026-11-20", 11300, 80), {}), ConflictError);
}

TEST_CASE("return charges: on-time return within allowance has no extras; notes never create damage") {
    Fleet f;
    const std::string b = f.book(f.swifts[0], R("2026-11-16", "2026-11-18")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10000, 60));
    const Invoice& inv =
        f.sys.rentals().completeReturn(b, Insp("2026-11-17", 10500, 70, "Big dent on the door"), {}); // early, 500 km
    CHECK_EQ(inv.additionalCharges(), Money());
    CHECK_EQ(inv.totalCharges(), Money::fromRupees(2700)); // no refund for early return
}

TEST_CASE("return validation: mileage cannot go down, dates must make sense") {
    Fleet f;
    const std::string b = f.book(f.swifts[0], R("2026-11-16", "2026-11-18")).id();
    CHECK_THROWS_AS(f.sys.rentals().completeReturn(b, Insp("2026-11-17", 10100, 50), {}), ConflictError); // not collected
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10050, 50));
    CHECK_THROWS_AS(f.sys.rentals().completeReturn(b, Insp("2026-11-17", 10049, 50), {}), ValidationError);
    CHECK_THROWS_AS(f.sys.rentals().previewReturn(b, Insp("2026-11-15", 10100, 50), {}), ValidationError);
    CHECK_THROWS_AS(DamageCharge("", Money::fromRupees(10)), ValidationError);
    CHECK_THROWS_AS(DamageCharge("Mirror", Money()), ValidationError);
    CHECK_THROWS_AS(InspectionReport(D("2026-11-17"), 10100, 101, ""), ValidationError);
    CHECK_EQ(f.sys.data().invoices.size(), std::size_t{0}); // nothing stored after failures
    CHECK_EQ(f.sys.rentals().booking(b).status(), BookingStatus::Active);
}

TEST_CASE("pickup: date window, odometer, double-hand-over and cancellation rules") {
    Fleet f;
    const std::string v = f.swifts[0];
    const std::string a = f.book(v, R("2026-11-16", "2026-11-18")).id();
    const std::string next = f.book(v, R("2026-11-18", "2026-11-20")).id();
    CHECK_THROWS_AS(f.sys.rentals().pickup(a, Insp("2026-11-15", 10000, 100)), ValidationError); // too early
    CHECK_THROWS_AS(f.sys.rentals().pickup(a, Insp("2026-11-16", 9999, 100)), ValidationError);  // odometer
    f.sys.rentals().pickup(a, Insp("2026-11-16", 10000, 100));
    CHECK_THROWS_AS(f.sys.rentals().cancelBooking(a), ConflictError); // already collected
    // The next customer cannot collect until the first rental is returned.
    CHECK_THROWS_AS(f.sys.rentals().pickup(next, Insp("2026-11-18", 10000, 100)), ConflictError);
    f.sys.rentals().completeReturn(a, Insp("2026-11-18", 10300, 100), {});
    f.sys.rentals().pickup(next, Insp("2026-11-18", 10300, 100));
    CHECK_EQ(f.sys.rentals().booking(next).status(), BookingStatus::Active);
}

TEST_CASE("maintenance: open record blocks booking and pickup; completion unblocks") {
    Fleet f;
    const std::string v = f.swifts[0];
    const std::string b = f.book(v, R("2026-11-16", "2026-11-18")).id();
    const MaintenanceRecord& r = f.sys.maintenance().openMaintenance(v, "Brake inspection", D("2026-11-10"));
    const std::string rid = r.id();
    CHECK_EQ(f.sys.maintenance().bookingsAffectedBy(v).size(), std::size_t{1});
    CHECK_THROWS_AS(f.sys.quotes().createQuote(f.customer, v, R("2026-11-20", "2026-11-21")), ConflictError);
    CHECK_THROWS_AS(f.sys.rentals().pickup(b, Insp("2026-11-16", 10000, 100)), ConflictError);
    CHECK_THROWS_AS(f.sys.maintenance().openMaintenance(v, "Again", D("2026-11-10")), ConflictError);

    f.sys.maintenance().completeMaintenance(rid, D("2026-11-12"), 10020, Money::fromRupees(3200));
    const Vehicle& veh = f.sys.fleet().vehicle(v);
    CHECK_EQ(veh.status(), VehicleStatus::Active);
    CHECK_EQ(veh.nextServiceDueKm(), 20020LL);
    CHECK_THROWS_AS(f.sys.maintenance().completeMaintenance(rid, D("2026-11-12"), 10020, Money()), ConflictError);
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10020, 100));
}

TEST_CASE("maintenance: a vehicle out on rental cannot be blocked") {
    Fleet f;
    const std::string b = f.book(f.swifts[0], R("2026-11-16", "2026-11-18")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10000, 100));
    CHECK_THROWS_AS(f.sys.maintenance().openMaintenance(f.swifts[0], "Oil change", D("2026-11-17")), ConflictError);
}

TEST_CASE("maintenance: service warning flags but does not block") {
    Fleet f;
    const std::string v = f.swifts[0];          // next service due at 20000 km
    f.sys.data().vehicles.get(v).recordOdometer(19600); // 400 km left (< 500 warning)
    const auto alerts = f.sys.maintenance().serviceAlerts();
    CHECK_EQ(alerts.size(), std::size_t{1});
    CHECK_EQ(alerts.front().kmRemaining, 400LL);
    CHECK(f.book(v, R("2026-11-16", "2026-11-18")).status() == BookingStatus::Confirmed);
}

TEST_CASE("history: deactivating a vehicle keeps its bookings and invoices") {
    Fleet f;
    const std::string v = f.swifts[0];
    const std::string b = f.book(v, R("2026-11-16", "2026-11-18")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-16", 10000, 100));
    const std::string inv = f.sys.rentals().completeReturn(b, Insp("2026-11-18", 10200, 100), {}).id();
    f.sys.fleet().deactivateVehicle(v);
    CHECK_EQ(f.sys.fleet().vehicle(v).status(), VehicleStatus::Inactive);
    CHECK_EQ(f.sys.rentals().booking(b).invoiceId(), inv);
    CHECK_EQ(f.sys.data().invoices.get(inv).totalCharges(), Money::fromRupees(2700));
}
