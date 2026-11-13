// Tests for value types and domain objects: Money, Date, DateRange,
// vehicles and customers.

#include "TestFramework.h"

#include "rental/Customer.h"
#include "rental/Date.h"
#include "rental/Errors.h"
#include "rental/Money.h"
#include "rental/VehicleModel.h"
#include "rental/VehicleTypes.h"

using namespace rental;

// ---------------------------------------------------------------- Money
TEST_CASE("money: formatting uses Indian grouping and two decimals") {
    CHECK_EQ(Money::fromRupees(1500).toString(), std::string("INR 1,500.00"));
    CHECK_EQ(Money::fromRupees(150000).toString(), std::string("INR 1,50,000.00"));
    CHECK_EQ(Money::fromPaise(12345678905).toString(), std::string("INR 12,34,56,789.05"));
    CHECK_EQ(Money::fromPaise(-50).toString(), std::string("-INR 0.50"));
    CHECK_EQ(Money::fromPaise(999).toString(), std::string("INR 9.99"));
}

TEST_CASE("money: parsing accepts rupees, paise and commas") {
    CHECK_EQ(Money::parse("1500"), Money::fromRupees(1500));
    CHECK_EQ(Money::parse("1,500.5"), Money::fromPaise(150050));
    CHECK_EQ(Money::parse("INR 2,500.75"), Money::fromPaise(250075));
    CHECK_THROWS_AS(Money::parse("12.345"), ValidationError);
    CHECK_THROWS_AS(Money::parse("-5"), ValidationError);
    CHECK_THROWS_AS(Money::parse("abc"), ValidationError);
    CHECK_THROWS_AS(Money::parse(""), ValidationError);
}

TEST_CASE("rounding: half away from zero to the nearest paisa") {
    CHECK_EQ(Money::fromPaise(1001).applyBasisPoints(5000), Money::fromPaise(501));   // 500.5 -> 501
    CHECK_EQ(Money::fromPaise(1003).applyBasisPoints(5000), Money::fromPaise(502));   // 501.5 -> 502
    CHECK_EQ(Money::fromPaise(-1001).applyBasisPoints(5000), Money::fromPaise(-501)); // symmetric
    CHECK_EQ(Money::fromPaise(1004).applyBasisPoints(3333), Money::fromPaise(335));   // 334.63 -> 335
    CHECK_EQ(multiplyBp(12500, 11500), BasisPoints{14375});
    CHECK_EQ(multiplyBp(9000, 11000), BasisPoints{9900});
    CHECK_EQ(formatFactor(11500), std::string("1.15x"));
    CHECK_EQ(formatFactor(14375), std::string("1.4375x"));
    CHECK_EQ(formatPercent(1000), std::string("10%"));
    CHECK_EQ(formatPercent(1250), std::string("12.5%"));
}

// ---------------------------------------------------------------- Date
TEST_CASE("dates: leap years and invalid calendar dates") {
    CHECK(Date::isLeapYear(2024));
    CHECK(Date::isLeapYear(2000));
    CHECK(!Date::isLeapYear(1900));
    CHECK(!Date::isLeapYear(2026));
    CHECK_EQ(Date::parse("2024-02-29").toString(), std::string("2024-02-29"));
    CHECK_EQ(Date::parse("2000-02-29").toString(), std::string("2000-02-29"));
    CHECK_THROWS_AS(Date::parse("2026-02-29"), ValidationError);
    CHECK_THROWS_AS(Date::parse("1900-02-29"), ValidationError);
    CHECK_THROWS_AS(Date::parse("2026-04-31"), ValidationError);
    CHECK_THROWS_AS(Date::parse("2026-13-01"), ValidationError);
    CHECK_THROWS_AS(Date::parse("2026-00-10"), ValidationError);
    CHECK_THROWS_AS(Date::parse("2026-1-05"), ValidationError);
    CHECK_THROWS_AS(Date::parse("05-01-2026"), ValidationError);
    CHECK_THROWS_AS(Date::parse("2026/01/05"), ValidationError);
    CHECK_THROWS_AS(Date::parse(""), ValidationError);
}

TEST_CASE("dates: arithmetic across months, leap days and years") {
    CHECK_EQ(Date::daysBetween(Date::parse("2024-01-01"), Date::parse("2025-01-01")), 366LL);
    CHECK_EQ(Date::daysBetween(Date::parse("2026-01-01"), Date::parse("2027-01-01")), 365LL);
    CHECK_EQ(Date::parse("2024-02-28").addDays(1), Date::parse("2024-02-29"));
    CHECK_EQ(Date::parse("2026-02-28").addDays(1), Date::parse("2026-03-01"));
    CHECK_EQ(Date::parse("2026-12-31").addDays(1), Date::parse("2027-01-01"));
    CHECK_EQ(Date::parse("2026-03-01").addDays(-1), Date::parse("2026-02-28"));
    CHECK_EQ(Date::parse("2026-11-10").weekdayName(), std::string("Tue"));
    CHECK_EQ(Date::parse("2024-02-29").weekdayName(), std::string("Thu"));
    CHECK(Date::parse("2026-11-14").isWeekend()); // Saturday
    CHECK(Date::parse("2026-11-15").isWeekend()); // Sunday
    CHECK(!Date::parse("2026-11-16").isWeekend());
}

TEST_CASE("date ranges: half-open, back-to-back allowed, overlap detected") {
    const DateRange a(Date::parse("2026-11-12"), Date::parse("2026-11-15"));
    CHECK_EQ(a.lengthDays(), 3LL);
    CHECK(a.contains(Date::parse("2026-11-14")));
    CHECK(!a.contains(Date::parse("2026-11-15"))); // return date excluded
    const DateRange backToBack(Date::parse("2026-11-15"), Date::parse("2026-11-17"));
    const DateRange overlapping(Date::parse("2026-11-14"), Date::parse("2026-11-16"));
    CHECK(!a.overlaps(backToBack));
    CHECK(!backToBack.overlaps(a));
    CHECK(a.overlaps(overlapping));
    CHECK_THROWS_AS(DateRange(Date::parse("2026-11-15"), Date::parse("2026-11-15")), ValidationError);
    CHECK_THROWS_AS(DateRange(Date::parse("2026-11-15"), Date::parse("2026-11-14")), ValidationError);
    const DateRange leap(Date::parse("2024-02-28"), Date::parse("2024-03-01"));
    CHECK_EQ(leap.lengthDays(), 2LL);
}

// ---------------------------------------------------------------- Vehicles & customers
TEST_CASE("vehicles: polymorphic behaviour and validation") {
    VehicleData d;
    d.id = "V-0001";
    d.registration = "mh-12 ab 1234";
    d.modelId = "M-0001";
    d.mileageKm = 1000;
    auto car = makeVehicle(VehicleCategory::Car, d, 5);
    d.id = "V-0002";
    auto bigBike = makeVehicle(VehicleCategory::Bike, d, 500);
    d.id = "V-0003";
    auto van = makeVehicle(VehicleCategory::Van, d, 800);

    CHECK_EQ(car->registration(), std::string("MH12AB1234"));
    CHECK(car->category() == VehicleCategory::Car);
    CHECK_EQ(car->securityDeposit(), Money::fromRupees(5000));
    CHECK_EQ(bigBike->securityDeposit(), Money::fromRupees(3000));
    CHECK_EQ(van->serviceIntervalKm(), 8000LL);
    CHECK_EQ(van->specSummary(), std::string("Payload kg: 800"));

    CHECK_THROWS_AS(car->recordOdometer(999), ValidationError);
    CHECK_THROWS_AS(car->setSpecValue(20), ValidationError);
    CHECK_THROWS_AS(makeVehicle(VehicleCategory::Car, d, 1), ValidationError);
    d.registration = "AB@1";
    CHECK_THROWS_AS(makeVehicle(VehicleCategory::Car, d, 5), ValidationError);

    car->startMaintenance();
    CHECK(!car->isRentable());
    CHECK_THROWS_AS(car->startMaintenance(), ConflictError);
    car->finishMaintenance();
    CHECK(car->isRentable());
}

TEST_CASE("customers: licence category and validity through the return date") {
    const DrivingLicence lic("dl-123", Date::parse("2026-12-10"), {VehicleCategory::Car});
    CHECK(lic.coversRental(VehicleCategory::Car, DateRange(Date::parse("2026-12-01"), Date::parse("2026-12-10"))));
    // Expires the day before the vehicle is due back.
    CHECK(!lic.coversRental(VehicleCategory::Car, DateRange(Date::parse("2026-12-05"), Date::parse("2026-12-11"))));
    CHECK(!lic.coversRental(VehicleCategory::Bike, DateRange(Date::parse("2026-12-01"), Date::parse("2026-12-03"))));
    CHECK_THROWS_AS(lic.requireCovers(VehicleCategory::Van,
                                      DateRange(Date::parse("2026-12-01"), Date::parse("2026-12-03"))),
                    ValidationError);
    CHECK_THROWS_AS(DrivingLicence("X1", Date::parse("2030-01-01"), {}), ValidationError);

    RegularCustomer c("C-0001", "Asha Rao", "+91 90000 00001", "asha@example.com", lic);
    CHECK_THROWS_AS(c.updateContact("Asha Rao", "12", "asha@example.com"), ValidationError);
    CHECK_THROWS_AS(c.updateContact("Asha Rao", "+91 90000 00001", "not-an-email"), ValidationError);
    CHECK_EQ(c.phone(), std::string("+91 90000 00001")); // unchanged after failed update
}

TEST_CASE("vehicle models: base rate must be positive") {
    VehicleModel swift("M-0001", "Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1500));
    CHECK_EQ(swift.displayName(), std::string("Maruti Swift"));
    CHECK_THROWS_AS(swift.setBaseDailyRate(Money()), ValidationError);
    CHECK_EQ(swift.baseDailyRate(), Money::fromRupees(1500));
}
