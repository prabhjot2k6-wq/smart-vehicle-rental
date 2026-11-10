// Pricing engine tests. Most build a PricingContext directly, so each rule
// can be tested in isolation with exact expected amounts.

#include "TestFramework.h"
#include "TestHelpers.h"

#include "rental/PricingStrategy.h"

using namespace rental;
using namespace testhelp;

namespace {

// Prices `period` with the same booked/capacity numbers on every day.
PriceBreakdown priceUniform(Money base, const DateRange& period, long long booked, long long capacity,
                            const PricingRules& rules = PricingRules::defaults(),
                            const PricingStrategy& strategy = DynamicPricingStrategy()) {
    std::vector<DemandDay> demand;
    for (Date d : period.days()) demand.push_back({d, booked, capacity});
    return strategy.price(PricingContext("Test model", base, period, rules, std::move(demand)));
}

const Money kSwift = Money::fromRupees(1500);
const Money kCreta = Money::fromRupees(2500);
const Money kInnova = Money::fromRupees(3500);

} // namespace

// Dates used below: 2026-11-16 Mon, 11-17 Tue, 11-18 Wed, 11-21 Sat, 11-22 Sun.

TEST_CASE("pricing: model-specific base rates on a neutral weekday") {
    // 1 of 2 booked = 50% -> demand 1.00x; Tuesday -> calendar 1.00x.
    CHECK_EQ(priceUniform(kSwift, R("2026-11-17", "2026-11-18"), 1, 2).total(), Money::fromRupees(1500));
    CHECK_EQ(priceUniform(kCreta, R("2026-11-17", "2026-11-18"), 1, 2).total(), Money::fromRupees(2500));
    CHECK_EQ(priceUniform(kInnova, R("2026-11-17", "2026-11-18"), 1, 2).total(), Money::fromRupees(3500));

    // Same through the real services: each model is priced from its own rate.
    Fleet f;
    f.book(f.innovas[0], R("2026-11-17", "2026-11-18")); // Innova 1/2 booked -> 1.00x
    f.book(f.cretas[0], R("2026-11-17", "2026-11-18"));  // Creta 1/2 booked -> 1.00x
    f.book(f.swifts[0], R("2026-11-17", "2026-11-18"));  // Swift 1/4 booked -> 25% -> 0.90x
    const auto& sys = f.sys;
    (void)sys;
    CHECK_EQ(f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.innovas[1]), R("2026-11-17", "2026-11-18")).total(),
             Money::fromRupees(3500));
    CHECK_EQ(f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.cretas[1]), R("2026-11-17", "2026-11-18")).total(),
             Money::fromRupees(2500));
    CHECK_EQ(f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.swifts[1]), R("2026-11-17", "2026-11-18")).total(),
             Money::fromRupees(1350));
}

TEST_CASE("pricing: every demand threshold boundary") {
    const PricingRules r = PricingRules::defaults();
    CHECK_EQ(r.demandFactor(0, 100).factorBp, BasisPoints{9000});
    CHECK_EQ(r.demandFactor(29, 100).factorBp, BasisPoints{9000});
    CHECK_EQ(r.demandFactor(30, 100).factorBp, BasisPoints{10000}); // exactly 30%
    CHECK_EQ(r.demandFactor(69, 100).factorBp, BasisPoints{10000});
    CHECK_EQ(r.demandFactor(70, 100).factorBp, BasisPoints{11500}); // exactly 70%
    CHECK_EQ(r.demandFactor(89, 100).factorBp, BasisPoints{11500});
    CHECK_EQ(r.demandFactor(90, 100).factorBp, BasisPoints{12500}); // exactly 90%
    CHECK_EQ(r.demandFactor(100, 100).factorBp, BasisPoints{12500});
    // Small fleets: exact integer comparison, no float rounding.
    CHECK_EQ(r.demandFactor(2, 3).factorBp, BasisPoints{10000});  // 66.7%
    CHECK_EQ(r.demandFactor(7, 10).factorBp, BasisPoints{11500}); // exactly 70%
    CHECK_EQ(r.demandFactor(3, 4).factorBp, BasisPoints{11500});  // 75%
    CHECK_EQ(r.demandFactor(9, 10).factorBp, BasisPoints{12500}); // exactly 90%

    // And through the strategy on a neutral Tuesday with Swift.
    CHECK_EQ(priceUniform(kSwift, R("2026-11-17", "2026-11-18"), 29, 100).total(), Money::fromRupees(1350));
    CHECK_EQ(priceUniform(kSwift, R("2026-11-17", "2026-11-18"), 70, 100).total(), Money::fromRupees(1725));
    CHECK_EQ(priceUniform(kSwift, R("2026-11-17", "2026-11-18"), 90, 100).total(), Money::fromPaise(187500));
}

TEST_CASE("pricing: weekend and holiday use the higher factor, never both") {
    PricingRules r = PricingRules::defaults();
    r.addHoliday(D("2026-11-18")); // Wednesday holiday
    r.addHoliday(D("2026-11-21")); // Saturday holiday
    // 50% demand -> 1.00x so only the calendar factor shows.
    const auto sat = priceUniform(kSwift, R("2026-11-22", "2026-11-23"), 1, 2, r);       // Sunday only
    const auto wedHol = priceUniform(kSwift, R("2026-11-18", "2026-11-19"), 1, 2, r);    // holiday only
    const auto satHol = priceUniform(kSwift, R("2026-11-21", "2026-11-22"), 1, 2, r);    // both
    CHECK_EQ(sat.days()[0].calendarFactorBp, BasisPoints{11000});
    CHECK_EQ(sat.total(), Money::fromRupees(1650));
    CHECK_EQ(wedHol.days()[0].calendarFactorBp, BasisPoints{11500});
    CHECK_EQ(wedHol.total(), Money::fromRupees(1725));
    CHECK_EQ(satHol.days()[0].calendarFactorBp, BasisPoints{11500}); // not 1.10 x 1.15 = 1.265
    CHECK_EQ(satHol.total(), Money::fromRupees(1725));
}

TEST_CASE("pricing: combined daily factor is capped at 1.40 before discount") {
    PricingRules r = PricingRules::defaults();
    r.addHoliday(D("2026-11-18"));
    // 90%+ demand (1.25) x holiday (1.15) = 1.4375 -> capped to 1.40.
    const auto capped = priceUniform(kInnova, R("2026-11-18", "2026-11-19"), 9, 10, r);
    CHECK(capped.days()[0].capped);
    CHECK_EQ(capped.days()[0].appliedFactorBp, BasisPoints{14000});
    CHECK_EQ(capped.total(), Money::fromRupees(4900));
    // 1.25 x weekend 1.10 = 1.375: under the cap.
    const auto notCapped = priceUniform(kInnova, R("2026-11-21", "2026-11-22"), 9, 10, r);
    CHECK(!notCapped.days()[0].capped);
    CHECK_EQ(notCapped.total(), Money::fromPaise(481250));
    // Cap applies per day, then the duration discount: 7 capped holiday days.
    PricingRules allHolidays = PricingRules::defaults();
    for (Date d : R("2026-11-16", "2026-11-23").days()) allHolidays.addHoliday(d);
    const auto week = priceUniform(kInnova, R("2026-11-16", "2026-11-23"), 9, 10, allHolidays);
    CHECK_EQ(week.subtotal(), Money::fromRupees(4900 * 7));
    CHECK_EQ(week.discount(), Money::fromRupees(3430));
    CHECK_EQ(week.total(), Money::fromRupees(30870));
}

TEST_CASE("pricing: duration discount boundaries, highest discount only") {
    const StandardPricingStrategy standard;
    const PricingRules r = PricingRules::defaults();
    const Money base = Money::fromRupees(1000);
    const Date s = D("2026-12-01");
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(6)), 0, 1, r, standard).discountBp(), BasisPoints{0});
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(7)), 0, 1, r, standard).discountBp(), BasisPoints{1000});
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(7)), 0, 1, r, standard).total(), Money::fromRupees(6300));
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(29)), 0, 1, r, standard).discountBp(), BasisPoints{1000});
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(30)), 0, 1, r, standard).discountBp(), BasisPoints{2000});
    CHECK_EQ(priceUniform(base, DateRange(s, s.addDays(30)), 0, 1, r, standard).total(), Money::fromRupees(24000));
    CHECK_EQ(r.durationDiscountBp(400), BasisPoints{2000}); // 20%, not 10% + 20%
    CHECK_THROWS_AS(PricingRules().setDurationDiscounts({{30, 2000}, {7, 1000}}), ValidationError);
}

TEST_CASE("rounding: daily amounts and discounts round half away from zero") {
    const Money oddBase = Money::fromPaise(155555); // INR 1,555.55
    // Tuesday, 0% demand: 155555 x 0.90 = 139999.5 paise -> 140000.
    const auto day = priceUniform(oddBase, R("2026-11-17", "2026-11-18"), 0, 4);
    CHECK_EQ(day.total(), Money::fromPaise(140000));
    // Standard: 7 x 155555 = 1088885; 10% = 108888.5 -> 108889; total 979996.
    const auto week = priceUniform(oddBase, R("2026-11-16", "2026-11-23"), 0, 4, PricingRules::defaults(),
                                   StandardPricingStrategy());
    CHECK_EQ(week.subtotal(), Money::fromPaise(1088885));
    CHECK_EQ(week.discount(), Money::fromPaise(108889));
    CHECK_EQ(week.total(), Money::fromPaise(979996));
}

TEST_CASE("pricing: zero rentable capacity is neutral, never divides by zero") {
    const PricingRules r = PricingRules::defaults();
    CHECK_EQ(r.demandFactor(0, 0).factorBp, BasisPoints{10000});
    CHECK_EQ(r.demandFactor(3, 0).factorBp, BasisPoints{10000});
    CHECK_EQ(priceUniform(kCreta, R("2026-11-17", "2026-11-18"), 0, 0).total(), Money::fromRupees(2500));

    // Through the services: both Cretas under maintenance -> capacity 0.
    Fleet f;
    f.sys.data().vehicles.get(f.cretas[0]).startMaintenance();
    f.sys.data().vehicles.get(f.cretas[1]).startMaintenance();
    const auto demand = f.sys.availability().modelDemand(f.creta, R("2026-11-17", "2026-11-19"));
    CHECK_EQ(demand.size(), std::size_t{2});
    CHECK_EQ(demand[0].capacity, 0LL);
}

TEST_CASE("pricing: utilization counts confirmed/active, excludes cancelled, unrentable and the proposal") {
    Fleet f;
    const DateRange week = R("2026-11-17", "2026-11-18"); // Tuesday
    f.book(f.swifts[0], week);
    const Booking& toCancel = f.book(f.swifts[1], week);
    f.sys.rentals().cancelBooking(toCancel.id());
    // 1 booked / 4 rentable. The proposed booking on swifts[2] is not counted.
    auto demand = f.sys.availability().modelDemand(f.swift, week);
    CHECK_EQ(demand[0].booked, 1LL);
    CHECK_EQ(demand[0].capacity, 4LL);
    // A vehicle under maintenance leaves the capacity.
    f.sys.data().vehicles.get(f.swifts[3]).startMaintenance();
    demand = f.sys.availability().modelDemand(f.swift, week);
    CHECK_EQ(demand[0].capacity, 3LL);
    const auto quote = f.sys.quotes().createQuote(f.customer, f.swifts[2], week);
    CHECK_EQ(quote.breakdown.days()[0].booked, 1LL);
    CHECK_EQ(quote.breakdown.days()[0].demandFactorBp, BasisPoints{10000}); // 1/3 = 33% -> 1.00x
}

TEST_CASE("pricing: demand is calculated separately for each day") {
    Fleet f;
    // Three of four Swifts booked only on Tue 17th -> 75% that day, 0% the next.
    for (int i = 0; i < 3; ++i) f.book(f.swifts[static_cast<std::size_t>(i)], R("2026-11-17", "2026-11-18"));
    const auto p = f.sys.quotes().priceFor(f.sys.fleet().vehicle(f.swifts[3]), R("2026-11-17", "2026-11-19"));
    CHECK_EQ(p.days()[0].demandFactorBp, BasisPoints{11500});
    CHECK_EQ(p.days()[1].demandFactorBp, BasisPoints{9000});
    CHECK_EQ(p.total(), Money::fromRupees(1725 + 1350));
}
