// Booking, quote and extension tests.

#include "TestFramework.h"
#include "TestHelpers.h"

using namespace rental;
using namespace testhelp;

TEST_CASE("bookings: overlapping rejected, back-to-back allowed") {
    Fleet f;
    const std::string v = f.swifts[0];
    f.book(v, R("2026-11-12", "2026-11-15"));
    CHECK_THROWS_AS(f.book(v, R("2026-11-14", "2026-11-16")), ConflictError); // overlaps the 14th
    CHECK_THROWS_AS(f.book(v, R("2026-11-10", "2026-11-20")), ConflictError); // surrounds it
    CHECK_THROWS_AS(f.book(v, R("2026-11-13", "2026-11-14")), ConflictError); // inside it
    const Booking& next = f.book(v, R("2026-11-15", "2026-11-17"));          // returns 15th, next picks up 15th
    CHECK_EQ(next.status(), BookingStatus::Confirmed);
    const Booking& before = f.book(v, R("2026-11-10", "2026-11-12"));
    CHECK(before.period().end() == D("2026-11-12"));
}

TEST_CASE("bookings: cancelled bookings free the vehicle; IDs are unique") {
    Fleet f;
    const Booking& a = f.book(f.swifts[0], R("2026-11-12", "2026-11-15"));
    const std::string firstId = a.id();
    f.sys.rentals().cancelBooking(firstId);
    CHECK_THROWS_AS(f.sys.rentals().cancelBooking(firstId), ConflictError);
    const Booking& b = f.book(f.swifts[0], R("2026-11-12", "2026-11-15"));
    CHECK(b.id() != firstId);
    CHECK_EQ(f.sys.rentals().booking(firstId).status(), BookingStatus::Cancelled);
}

TEST_CASE("quotes: licence must cover the category and stay valid to the return date") {
    Fleet f;
    const std::string bikeOnly = f.addCustomer("BIKE-ONLY", D("2035-01-01"), {VehicleCategory::Bike});
    CHECK_THROWS_AS(f.sys.quotes().createQuote(bikeOnly, f.swifts[0], R("2026-11-12", "2026-11-14")), ValidationError);
    const std::string expiring = f.addCustomer("EXPIRING", D("2026-11-13"), {VehicleCategory::Car});
    CHECK_THROWS_AS(f.sys.quotes().createQuote(expiring, f.swifts[0], R("2026-11-12", "2026-11-14")), ValidationError);
    // Valid on the return date itself is enough.
    CHECK_EQ(f.sys.quotes().createQuote(expiring, f.swifts[0], R("2026-11-12", "2026-11-13")).customerId, expiring);
}

TEST_CASE("quotes: past dates and unavailable vehicles are rejected") {
    Fleet f;
    CHECK_THROWS_AS(f.sys.quotes().createQuote(f.customer, f.swifts[0], R("2026-11-08", "2026-11-10")),
                    ValidationError);
    f.sys.data().vehicles.get(f.swifts[0]).startMaintenance();
    CHECK_THROWS_AS(f.sys.quotes().createQuote(f.customer, f.swifts[0], R("2026-11-12", "2026-11-14")), ConflictError);
}

TEST_CASE("quotes: expire after 15 minutes; confirmation rejects expired quotes") {
    Fleet f;
    const std::string ok = f.sys.quotes().createQuote(f.customer, f.swifts[0], R("2026-11-12", "2026-11-14")).id;
    const std::string late = f.sys.quotes().createQuote(f.customer, f.swifts[1], R("2026-11-12", "2026-11-14")).id;
    f.clock.advanceSeconds(15 * 60 - 1); // 14:59 later
    CHECK_EQ(f.sys.rentals().confirmQuote(ok).vehicleId(), f.swifts[0]);
    f.clock.advanceSeconds(1); // exactly 15:00
    CHECK_THROWS_AS(f.sys.rentals().confirmQuote(late), ValidationError);
    CHECK(f.sys.quotes().findPending(late) == nullptr); // an expired quote cannot be reused
    // Validity is configurable.
    f.sys.data().pricingRules.setQuoteValidityMinutes(1);
    const std::string q = f.sys.quotes().createQuote(f.customer, f.swifts[2], R("2026-11-12", "2026-11-14")).id;
    f.clock.advanceSeconds(61);
    CHECK_THROWS_AS(f.sys.rentals().confirmQuote(q), ValidationError);
}

TEST_CASE("quotes: confirmation re-checks availability") {
    Fleet f;
    const std::string q1 = f.sys.quotes().createQuote(f.customer, f.swifts[0], R("2026-11-12", "2026-11-14")).id;
    const std::string q2 = f.sys.quotes().createQuote(f.customer, f.swifts[0], R("2026-11-13", "2026-11-15")).id;
    f.sys.rentals().confirmQuote(q1);
    CHECK_THROWS_AS(f.sys.rentals().confirmQuote(q2), ConflictError);
}

TEST_CASE("quotes: an unexpired quote is honoured even if demand rises") {
    Fleet f;
    const DateRange tue = R("2026-11-17", "2026-11-18");
    const auto& quote = f.sys.quotes().createQuote(f.customer, f.swifts[3], tue);
    const std::string quoteId = quote.id;
    const Money quoted = quote.breakdown.total();
    CHECK_EQ(quoted, Money::fromRupees(1350)); // 0% demand -> 0.90x
    for (int i = 0; i < 3; ++i) f.book(f.swifts[static_cast<std::size_t>(i)], tue); // now 75%
    CHECK_EQ(f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.swifts[3]), tue).total(), Money::fromRupees(1725));
    CHECK_EQ(f.sys.rentals().confirmQuote(quoteId).agreedRentalTotal(), quoted);
}

TEST_CASE("bookings: confirmed price preserved after rate and rule changes") {
    Fleet f;
    const DateRange period = R("2026-11-20", "2026-11-23"); // Fri, Sat, Sun
    const Booking& b = f.book(f.cretas[0], period);
    const std::string id = b.id();
    const Money before = b.agreedRentalTotal();
    // 0/2 booked -> 0.90 every day: Fri 2250, Sat/Sun 2500x0.99=2475 -> 7200.
    CHECK_EQ(before, Money::fromRupees(7200));

    f.sys.fleet().setModelRate(f.creta, Money::fromRupees(4000));
    f.sys.data().pricingRules.setWeekendFactorBp(13000);
    f.sys.data().pricingRules.addHoliday(D("2026-11-20"));
    f.sys.quotes().setStrategy(std::make_unique<StandardPricingStrategy>());

    const Booking& after = f.sys.rentals().booking(id);
    CHECK_EQ(after.agreedRentalTotal(), before);
    CHECK_EQ(after.priceParts()[0].baseDailyRate(), Money::fromRupees(2500));
    CHECK_EQ(after.priceParts()[0].days()[1].calendarFactorBp, BasisPoints{11000});
    // A new quote for the other Creta uses the new rules.
    CHECK_EQ(f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.cretas[1]), period).total(), Money::fromRupees(12000));
}

TEST_CASE("extensions: priced separately, ignore themselves, and detect conflicts") {
    Fleet f;
    const std::string v = f.swifts[0];
    const std::string a = f.book(v, R("2026-11-16", "2026-11-18")).id();
    const std::string later = f.book(v, R("2026-11-20", "2026-11-22")).id();

    // Extending A to the 20th is back-to-back with the later booking: allowed.
    const PriceQuote q = f.sys.rentals().quoteExtension(a, D("2026-11-20"));
    CHECK(q.breakdown.period() == R("2026-11-18", "2026-11-20"));
    CHECK_EQ(q.breakdown.total(), Money::fromRupees(1350 * 2)); // 18th, 19th at 0.90x
    const Booking& extended = f.sys.rentals().confirmExtension(q);
    CHECK(extended.period() == R("2026-11-16", "2026-11-20"));
    CHECK_EQ(extended.priceParts().size(), std::size_t{2});
    CHECK_EQ(extended.agreedRentalTotal(), Money::fromRupees(1350 * 4));

    // One more day would overlap the later booking.
    CHECK_THROWS_AS(f.sys.rentals().quoteExtension(a, D("2026-11-21")), ConflictError);
    // Return date must move forward.
    CHECK_THROWS_AS(f.sys.rentals().quoteExtension(a, D("2026-11-19")), ValidationError);
    // Cancelled bookings cannot be extended.
    f.sys.rentals().cancelBooking(later);
    CHECK_THROWS_AS(f.sys.rentals().quoteExtension(later, D("2026-11-25")), ConflictError);
    // With the later booking cancelled, the extension now succeeds.
    CHECK(f.sys.rentals().confirmExtension(f.sys.rentals().quoteExtension(a, D("2026-11-23"))).period().end() ==
          D("2026-11-23"));
}

TEST_CASE("extensions: a stale or expired extension quote is rejected") {
    Fleet f;
    const std::string a = f.book(f.swifts[0], R("2026-11-16", "2026-11-18")).id();
    const PriceQuote q1 = f.sys.rentals().quoteExtension(a, D("2026-11-19"));
    const PriceQuote q2 = f.sys.rentals().quoteExtension(a, D("2026-11-20"));
    f.sys.rentals().confirmExtension(q1);
    CHECK_THROWS_AS(f.sys.rentals().confirmExtension(q2), ConflictError); // booking changed since q2
    const PriceQuote q3 = f.sys.rentals().quoteExtension(a, D("2026-11-21"));
    f.clock.advanceSeconds(16 * 60);
    CHECK_THROWS_AS(f.sys.rentals().confirmExtension(q3), ValidationError);
}

TEST_CASE("extensions: licence must remain valid through the new return date") {
    Fleet f;
    const std::string c = f.addCustomer("SHORT-LIC", D("2026-11-18"), {VehicleCategory::Car});
    const std::string q = f.sys.quotes().createQuote(c, f.swifts[0], R("2026-11-16", "2026-11-18")).id;
    const std::string b = f.sys.rentals().confirmQuote(q).id();
    CHECK_THROWS_AS(f.sys.rentals().quoteExtension(b, D("2026-11-19")), ValidationError);
}

TEST_CASE("fleet: deactivation refused while bookings are open") {
    Fleet f;
    const std::string b = f.book(f.swifts[0], R("2026-11-16", "2026-11-18")).id();
    CHECK_THROWS_AS(f.sys.fleet().deactivateVehicle(f.swifts[0]), ConflictError);
    f.sys.rentals().cancelBooking(b);
    f.sys.fleet().deactivateVehicle(f.swifts[0]);
    CHECK_EQ(f.sys.rentals().booking(b).status(), BookingStatus::Cancelled); // history kept
    CHECK_THROWS_AS(f.book(f.swifts[0], R("2026-11-20", "2026-11-21")), ConflictError);
}

TEST_CASE("search: filters by category, model and budget, cheapest first") {
    Fleet f;
    const DateRange tue = R("2026-11-17", "2026-11-18");
    SearchCriteria all{tue, std::nullopt, "", std::nullopt};
    CHECK_EQ(f.sys.quotes().search(all).size(), std::size_t{8});
    SearchCriteria byModel{tue, std::nullopt, f.innova, std::nullopt};
    CHECK_EQ(f.sys.quotes().search(byModel).size(), std::size_t{2});
    SearchCriteria budget{tue, VehicleCategory::Car, "", Money::fromRupees(2250)};
    const auto cheap = f.sys.quotes().search(budget); // Swift 1350 x4, Creta 2250 x2
    CHECK_EQ(cheap.size(), std::size_t{6});
    CHECK_EQ(cheap.front().price.total(), Money::fromRupees(1350));
    f.book(f.swifts[0], tue);
    CHECK_EQ(f.sys.quotes().search(all).size(), std::size_t{7});
    SearchCriteria bikes{tue, VehicleCategory::Bike, "", std::nullopt};
    CHECK(f.sys.quotes().search(bikes).empty());
}
