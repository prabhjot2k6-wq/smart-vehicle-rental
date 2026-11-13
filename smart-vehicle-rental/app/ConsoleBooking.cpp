// Menus 3 (availability search & quotes) and 4 (booking management).
#include "ConsoleApp.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <iostream>

namespace rental {

namespace {
std::string remaining(std::int64_t expiresAt, std::int64_t now) {
    if (now >= expiresAt) return "EXPIRED";
    const std::int64_t s = expiresAt - now;
    return std::to_string(s / 60) + "m " + std::to_string(s % 60) + "s";
}
} // namespace

std::string ConsoleApp::askBookingId() {
    const std::string id = toUpper(io_.askText("Booking ID (e.g. B-0001)"));
    sys_.rentals().booking(id);
    return id;
}

void ConsoleApp::printBreakdown(const PriceBreakdown& b) {
    io_.info(b.strategyName() + " pricing, base rate " + b.baseDailyRate().toString() + "/day, " +
             b.period().toString());
    TextTable t({"Date", "Day", "Model utilization", "Demand", "Calendar", "Applied", "Amount"});
    t.alignRight(6);
    for (const DailyPrice& d : b.days()) {
        const std::string util = d.capacity == 0 ? "no capacity" : d.demandReason;
        t.addRow({d.date.toString(), d.date.weekdayName(), util, formatFactor(d.demandFactorBp),
                  formatFactor(d.calendarFactorBp) + " " + d.calendarReason,
                  formatFactor(d.appliedFactorBp) + (d.capped ? " (capped)" : ""), d.amount.toString()});
    }
    t.print(io_.out());
    io_.out() << "  Subtotal:          " << b.subtotal() << "\n";
    if (b.discountBp() > 0) {
        io_.out() << "  Duration discount: -" << b.discount() << " (" << formatPercent(b.discountBp()) << " for "
                  << b.period().lengthDays() << " days)\n";
    }
    io_.out() << "  TOTAL:             " << b.total() << "\n";
    io_.info("  (Rounding: each amount is rounded half away from zero to the nearest paisa.)");
}

void ConsoleApp::printBookingSummary(const Booking& b) {
    const Vehicle& v = sys_.fleet().vehicle(b.vehicleId());
    const Customer& c = sys_.customers().customer(b.customerId());
    io_.out() << "Booking " << b.id() << " [" << toString(b.status()) << "]\n"
              << "  Customer: " << c.id() << " " << c.name() << "\n"
              << "  Vehicle:  " << v.id() << " " << v.registration() << " (" << sys_.fleet().modelOf(v).displayName()
              << ")\n"
              << "  Period:   " << b.period().toString() << "\n"
              << "  Agreed rental total: " << b.agreedRentalTotal() << "\n";
}

// ===================================================================== 3. quotes
void ConsoleApp::quoteMenu() {
    while (true) {
        const int c = io_.menu("Availability search and price quotes",
                               {"Search available vehicles", "Create price quote for a customer",
                                "Confirm a pending quote", "List pending quotes", "Pricing settings"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { searchAvailability(); }); break;
        case 2: runAction([this] { createQuote(); }); break;
        case 3: runAction([this] { confirmPendingQuote(); }); break;
        case 4: runAction([this] { listPendingQuotes(); }); break;
        case 5: runAction([this] { pricingSettings(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::searchAvailability() {
    io_.heading("Search available vehicles");
    const Date start = io_.askDateDefault("Pickup date", sys_.clock().today());
    const Date end = io_.askDate("Return date (not billed)");
    SearchCriteria criteria{DateRange(start, end), std::nullopt, "", std::nullopt};
    const std::string cat = io_.askOptionalText("Category Car/Bike/Van");
    if (!cat.empty()) criteria.category = parseVehicleCategory(cat);
    criteria.modelId = toUpper(io_.askOptionalText("Model ID, e.g. M-0001"));
    if (!criteria.modelId.empty()) sys_.data().models.get(criteria.modelId);
    const std::string budget = io_.askOptionalText("Maximum total budget INR");
    if (!budget.empty()) criteria.maxTotal = Money::parse(budget);

    const auto results = sys_.quotes().search(criteria);
    TextTable t({"Vehicle", "Registration", "Model", "Type", "Days", "Subtotal", "Discount", "Total"});
    for (std::size_t col = 4; col <= 7; ++col) t.alignRight(col);
    for (const SearchResult& r : results) {
        t.addRow({r.vehicle->id(), r.vehicle->registration(), r.model->displayName(), toString(r.model->category()),
                  std::to_string(r.price.period().lengthDays()), r.price.subtotal().toString(),
                  r.price.discount().toString(), r.price.total().toString()});
    }
    t.print(io_.out());
    io_.info(std::to_string(results.size()) + " vehicle(s) available. Prices are previews; create a quote to hold one for " +
             std::to_string(sys_.data().pricingRules.quoteValidityMinutes()) + " minutes.");
}

void ConsoleApp::createQuote() {
    io_.heading("Create price quote");
    const std::string customerId = askCustomerId();
    const std::string vehicleId = askVehicleId();
    const Date start = io_.askDateDefault("Pickup date", sys_.clock().today());
    const Date end = io_.askDate("Return date (not billed)");
    const PriceQuote& q = sys_.quotes().createQuote(customerId, vehicleId, DateRange(start, end));
    io_.info("Quote " + q.id + " for customer " + q.customerId + ", vehicle " + q.vehicleId + ":");
    printBreakdown(q.breakdown);
    io_.info("Valid for " + remaining(q.expiresAt, sys_.quotes().now()) + ".");
    const std::string quoteId = q.id; // q may be removed by confirmQuote
    if (io_.confirm("Confirm this booking now?")) {
        const Booking& b = sys_.rentals().confirmQuote(quoteId);
        dataChanged();
        io_.info("Booking confirmed.");
        printBookingSummary(b);
    } else {
        io_.info("Quote kept as pending. Confirm it later from this menu before it expires.");
    }
}

void ConsoleApp::confirmPendingQuote() {
    listPendingQuotes();
    if (sys_.quotes().pendingQuotes().empty()) return;
    const std::string id = toUpper(io_.askText("Quote ID (e.g. Q-1)"));
    const PriceQuote* q = sys_.quotes().findPending(id);
    if (q == nullptr) throw NotFoundError("Quote " + id + " not found.");
    printBreakdown(q->breakdown);
    if (!io_.confirm("Confirm booking at " + q->breakdown.total().toString() + "?")) throw InputCancelled{};
    const Booking& b = sys_.rentals().confirmQuote(id);
    dataChanged();
    io_.info("Booking confirmed.");
    printBookingSummary(b);
}

void ConsoleApp::listPendingQuotes() {
    TextTable t({"Quote", "Customer", "Vehicle", "Period", "Total", "Expires in"});
    t.alignRight(4);
    const std::int64_t now = sys_.quotes().now();
    for (const PriceQuote* q : sys_.quotes().pendingQuotes()) {
        t.addRow({q->id, q->customerId, q->vehicleId, q->breakdown.period().toString(), q->breakdown.total().toString(),
                  remaining(q->expiresAt, now)});
    }
    t.print(io_.out());
}

void ConsoleApp::pricingSettings() {
    while (true) {
        PricingRules& r = sys_.data().pricingRules;
        io_.heading("Pricing settings");
        io_.out() << "  Active strategy: " << sys_.quotes().strategy().name() << "\n  Demand tiers: ";
        for (const DemandTier& t : r.demandTiers()) {
            io_.out() << ">=" << formatPercent(t.minUtilizationBp) << " -> " << formatFactor(t.factorBp) << "  ";
        }
        io_.out() << "\n  Weekend " << formatFactor(r.weekendFactorBp()) << ", holiday " << formatFactor(r.holidayFactorBp())
                  << " (higher one if both), daily cap " << formatFactor(r.maxDailyFactorBp()) << "\n  Duration discounts: ";
        for (const DurationDiscount& d : r.durationDiscounts()) {
            io_.out() << d.minDays << "+ days -> " << formatPercent(d.discountBp) << "  ";
        }
        io_.out() << "\n  Quote validity: " << r.quoteValidityMinutes() << " minutes\n  Holidays: ";
        for (Date h : r.holidays()) io_.out() << h << " ";
        io_.out() << (r.holidays().empty() ? "(none)" : "") << "\n";

        const int c = io_.menu("Change", {"Switch pricing strategy (Dynamic / Standard)", "Add holiday",
                                          "Remove holiday", "Set weekend factor", "Set holiday factor",
                                          "Set quote validity (minutes)"});
        if (c == 0) return;
        runAction([&] {
            switch (c) {
            case 1:
                if (sys_.quotes().strategy().name() == "Dynamic") {
                    sys_.quotes().setStrategy(std::make_unique<StandardPricingStrategy>());
                } else {
                    sys_.quotes().setStrategy(std::make_unique<DynamicPricingStrategy>());
                }
                io_.info("Strategy is now " + sys_.quotes().strategy().name() +
                         " (applies to new quotes; not saved - Dynamic is the default at startup).");
                break;
            case 2: r.addHoliday(io_.askDate("Holiday date")); dataChanged(); break;
            case 3:
                if (!r.removeHoliday(io_.askDate("Holiday date"))) io_.error("That date is not a configured holiday.");
                else dataChanged();
                break;
            case 4:
                r.setWeekendFactorBp(io_.askInt("Weekend factor in basis points (11000 = 1.10x)", 1, 50000));
                dataChanged();
                break;
            case 5:
                r.setHolidayFactorBp(io_.askInt("Holiday factor in basis points (11500 = 1.15x)", 1, 50000));
                dataChanged();
                break;
            case 6:
                r.setQuoteValidityMinutes(static_cast<int>(io_.askInt("Minutes", 1, 1440)));
                dataChanged();
                break;
            default: break;
            }
        });
    }
}

// ===================================================================== 4. bookings
void ConsoleApp::bookingMenu() {
    while (true) {
        const int c = io_.menu("Booking management", {"List bookings", "View booking details", "Extend booking",
                                                      "Cancel booking", "Confirm a pending quote"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { listBookings(); }); break;
        case 2: runAction([this] { viewBooking(); }); break;
        case 3: runAction([this] { extendBooking(); }); break;
        case 4: runAction([this] { cancelBooking(); }); break;
        case 5: runAction([this] { confirmPendingQuote(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::listBookings() {
    TextTable t({"Booking", "Status", "Customer", "Vehicle", "Pickup", "Return", "Days", "Agreed total"});
    t.alignRight(6);
    t.alignRight(7);
    for (const Booking* b : sys_.rentals().bookings()) {
        t.addRow({b->id(), toString(b->status()), b->customerId(), b->vehicleId(), b->period().start().toString(),
                  b->period().end().toString(), std::to_string(b->period().lengthDays()),
                  b->agreedRentalTotal().toString()});
    }
    t.print(io_.out());
}

void ConsoleApp::viewBooking() {
    const Booking& b = sys_.rentals().booking(askBookingId());
    printBookingSummary(b);
    for (std::size_t i = 0; i < b.priceParts().size(); ++i) {
        io_.heading(i == 0 ? "Original agreed price" : "Extension " + std::to_string(i));
        printBreakdown(b.priceParts()[i]);
    }
    const auto printInspection = [this](const char* label, const InspectionReport& r) {
        io_.out() << "  " << label << ": " << r.date() << ", odometer " << r.odometerKm() << " km, fuel "
                  << r.fuelPercent() << "%, notes: " << (r.notes().empty() ? "-" : r.notes()) << "\n";
    };
    if (b.pickupReport()) printInspection("Pickup", *b.pickupReport());
    if (b.returnReport()) printInspection("Return", *b.returnReport());
    if (!b.depositHeld().isZero()) io_.out() << "  Deposit held: " << b.depositHeld() << "\n";
    if (!b.invoiceId().empty()) io_.out() << "  Invoice: " << b.invoiceId() << "\n";
}

void ConsoleApp::extendBooking() {
    const std::string id = askBookingId();
    const Booking& b = sys_.rentals().booking(id);
    printBookingSummary(b);
    const Date newEnd = io_.askDate("New return date");
    const PriceQuote q = sys_.rentals().quoteExtension(id, newEnd);
    io_.heading("Extension price (only the extra days)");
    printBreakdown(q.breakdown);
    io_.info("Additional amount payable: " + q.breakdown.total().toString());
    if (!io_.confirm("Extend booking " + id + " to " + newEnd.toString() + "?")) throw InputCancelled{};
    const Booking& updated = sys_.rentals().confirmExtension(q);
    dataChanged();
    io_.info("Extended.");
    printBookingSummary(updated);
}

void ConsoleApp::cancelBooking() {
    const std::string id = askBookingId();
    printBookingSummary(sys_.rentals().booking(id));
    io_.info("Cancellation cannot be undone (no cancellation fee in this version).");
    if (!io_.confirm("Cancel booking " + id + "?")) throw InputCancelled{};
    sys_.rentals().cancelBooking(id);
    dataChanged();
    io_.info("Booking " + id + " cancelled.");
}

} // namespace rental
