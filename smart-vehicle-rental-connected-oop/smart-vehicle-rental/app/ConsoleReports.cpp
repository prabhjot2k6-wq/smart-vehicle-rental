// Menu 7: reports (read-only).
#include "ConsoleApp.h"

#include <iostream>

namespace rental {

void ConsoleApp::reportsMenu() {
    while (true) {
        const int c = io_.menu("Reports", {"Fleet availability on a date", "Active rentals", "Maintenance due",
                                           "Revenue from finalized invoices", "All bookings (history)"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { fleetReport(); }); break;
        case 2: runAction([this] { activeRentalsReport(); }); break;
        case 3: runAction([this] { maintenanceDueReport(); }); break;
        case 4: runAction([this] { revenueReport(); }); break;
        case 5: runAction([this] { listBookings(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::fleetReport() {
    const Date day = io_.askDateDefault("Date", sys_.clock().today());
    TextTable t({"Vehicle", "Registration", "Model", "Type", "State on " + day.toString()});
    int available = 0;
    for (const FleetStatusRow& r : sys_.reports().fleetAvailability(day)) {
        available += r.state == "Available" ? 1 : 0;
        t.addRow({r.vehicle->id(), r.vehicle->registration(), r.model->displayName(), toString(r.vehicle->category()),
                  r.state});
    }
    t.print(io_.out());
    io_.info(std::to_string(available) + " vehicle(s) available on " + day.toString() + ".");
}

void ConsoleApp::activeRentalsReport() {
    const Date today = sys_.clock().today();
    TextTable t({"Booking", "Customer", "Vehicle", "Picked up", "Due back", "Status"});
    for (const ActiveRentalRow& r : sys_.reports().activeRentals(today)) {
        const Customer& c = sys_.customers().customer(r.booking->customerId());
        t.addRow({r.booking->id(), c.id() + " " + c.name(), r.booking->vehicleId(),
                  r.booking->pickupReport()->date().toString(), r.booking->period().end().toString(),
                  r.overdue ? "OVERDUE by " + std::to_string(r.daysLate) + " day(s)" : "On time"});
    }
    t.print(io_.out());
}

void ConsoleApp::maintenanceDueReport() {
    io_.heading("Blocked now (open maintenance)");
    TextTable open({"Record", "Vehicle", "Since", "Task"});
    for (const MaintenanceRecord* r : sys_.maintenance().records()) {
        if (r->status() == MaintenanceStatus::Open) {
            open.addRow({r->id(), r->vehicleId(), r->openedOn().toString(), r->description()});
        }
    }
    open.print(io_.out());
    io_.heading("Service warnings (still bookable)");
    serviceWarnings();
}

void ConsoleApp::revenueReport() {
    std::optional<DateRange> period;
    if (io_.confirm("Limit to a date range? (otherwise all time)")) {
        const Date from = io_.askDate("From (inclusive)");
        const Date to = io_.askDate("To (exclusive)");
        period = DateRange(from, to);
    }
    const RevenueSummary s = sys_.reports().revenue(period);
    io_.heading("Revenue " + (period ? period->toString() : std::string("(all time)")));
    io_.out() << "  Finalized invoices:  " << s.invoiceCount << "\n"
              << "  Rental charges:      " << s.rentalCharges << "\n"
              << "  Additional charges:  " << s.additionalCharges << "\n"
              << "  TOTAL REVENUE:       " << s.total << "\n"
              << "  (Refundable deposits are not revenue and are excluded.)\n";
}

} // namespace rental
