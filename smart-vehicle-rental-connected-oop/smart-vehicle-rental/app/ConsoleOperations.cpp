// Menus 5 (pickup and return) and 6 (maintenance).
#include "ConsoleApp.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <algorithm>
#include <iostream>

namespace rental {

// ===================================================================== 5. pickup & return
void ConsoleApp::operationsMenu() {
    while (true) {
        const int c = io_.menu("Pickup and return",
                               {"Collect vehicle (pickup)", "Return vehicle and invoice", "View invoice", "List invoices"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { collectVehicle(); }); break;
        case 2: runAction([this] { returnVehicle(); }); break;
        case 3: runAction([this] { viewInvoice(); }); break;
        case 4: runAction([this] { listInvoices(); }); break;
        default: break;
        }
    }
}

InspectionReport ConsoleApp::askInspection(const Vehicle& vehicle, long long minimumOdometer, int defaultFuel) {
    while (true) {
        const Date date = io_.askDateDefault("Date", sys_.clock().today());
        const long long km = io_.askIntDefault("Odometer km", minimumOdometer, 5000000,
                                               std::max(minimumOdometer, vehicle.mileageKm()));
        const int fuel = static_cast<int>(io_.askIntDefault("Fuel/charge level %", 0, 100, defaultFuel));
        const std::string notes = io_.askOptionalText("Inspection notes", "blank = none");
        try {
            return InspectionReport(date, km, fuel, notes);
        } catch (const ValidationError& e) {
            io_.error(e.what());
        }
    }
}

void ConsoleApp::collectVehicle() {
    io_.heading("Collect vehicle");
    const std::string id = askBookingId();
    const Booking& b = sys_.rentals().booking(id);
    printBookingSummary(b);
    const Vehicle& v = sys_.fleet().vehicle(b.vehicleId());
    const InspectionReport report = askInspection(v, 0, 100);
    io_.info("Refundable deposit to collect (simulated): " + v.securityDeposit().toString() +
             ". It is kept separate from rental charges.");
    if (!io_.confirm("Hand over " + v.id() + " to the customer?")) throw InputCancelled{};
    sys_.rentals().pickup(id, report);
    dataChanged();
    io_.info("Booking " + id + " is now Active. Vehicle due back on " + b.period().end().toString() + ".");
}

void ConsoleApp::returnVehicle() {
    io_.heading("Return vehicle");
    const std::string id = askBookingId();
    const Booking& b = sys_.rentals().booking(id);
    printBookingSummary(b);
    if (b.status() != BookingStatus::Active || !b.pickupReport()) {
        throw ConflictError("Booking " + id + " is " + toString(b.status()) + "; only Active bookings can be returned.");
    }
    const InspectionReport& pick = *b.pickupReport();
    io_.info("At pickup: odometer " + std::to_string(pick.odometerKm()) + " km, fuel " +
             std::to_string(pick.fuelPercent()) + "%.");
    const Vehicle& v = sys_.fleet().vehicle(b.vehicleId());
    const InspectionReport report = askInspection(v, pick.odometerKm(), pick.fuelPercent());

    std::vector<DamageCharge> damages;
    io_.info("Damage charges are entered manually; they are never guessed from the notes.");
    while (io_.confirm("Add a damage charge?")) {
        try {
            const std::string reason = io_.askText("Damage reason");
            damages.emplace_back(reason, io_.askMoney("Damage amount"));
        } catch (const ValidationError& e) {
            io_.error(e.what());
        }
    }

    io_.heading("Invoice preview (nothing saved yet)");
    printInvoice(sys_.rentals().previewReturn(id, report, damages));
    if (!io_.confirm("Finalize this invoice and complete the return? This cannot be undone")) throw InputCancelled{};
    const Invoice& invoice = sys_.rentals().completeReturn(id, report, damages);
    dataChanged();
    io_.heading("Final invoice");
    printInvoice(invoice);
}

void ConsoleApp::printInvoice(const Invoice& inv) {
    const Customer& c = sys_.customers().customer(inv.customerId());
    const Vehicle& v = sys_.fleet().vehicle(inv.vehicleId());
    io_.out() << "Invoice " << inv.id() << " [" << toString(inv.status()) << "]  issued " << inv.issueDate() << "\n"
              << "  Booking " << inv.bookingId() << ", customer " << c.id() << " " << c.name() << ", vehicle " << v.id()
              << " " << v.registration() << " (" << sys_.fleet().modelOf(v).displayName() << ")\n";
    TextTable t({"#", "Type", "Description", "Amount"});
    t.alignRight(3);
    int n = 1;
    for (const InvoiceLine& l : inv.lines()) {
        t.addRow({std::to_string(n++), toString(l.type), l.description, l.amount.toString()});
    }
    t.print(io_.out());
    io_.out() << "  Rental charges:      " << inv.rentalCharges() << "\n"
              << "  Additional charges:  " << inv.additionalCharges() << "\n"
              << "  TOTAL PAYABLE:       " << inv.totalCharges() << "  (simulated payment)\n"
              << "  Deposit held:        " << inv.depositHeld() << " -> refunded " << inv.depositRefunded()
              << " (not revenue)\n";
}

void ConsoleApp::viewInvoice() {
    const std::string id = toUpper(io_.askText("Invoice ID (e.g. INV-0001)"));
    printInvoice(sys_.data().invoices.get(id));
}

void ConsoleApp::listInvoices() {
    TextTable t({"Invoice", "Status", "Issued", "Booking", "Customer", "Vehicle", "Total charges", "Deposit refunded"});
    t.alignRight(6);
    t.alignRight(7);
    for (const Invoice* i : sys_.data().invoices.all()) {
        t.addRow({i->id(), toString(i->status()), i->issueDate().toString(), i->bookingId(), i->customerId(),
                  i->vehicleId(), i->totalCharges().toString(), i->depositRefunded().toString()});
    }
    t.print(io_.out());
}

// ===================================================================== 6. maintenance
void ConsoleApp::maintenanceMenu() {
    while (true) {
        const int c = io_.menu("Maintenance", {"List maintenance records", "Open maintenance (blocks the vehicle)",
                                               "Complete maintenance", "Service warnings (approaching threshold)"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { listMaintenance(); }); break;
        case 2: runAction([this] { openMaintenance(); }); break;
        case 3: runAction([this] { completeMaintenance(); }); break;
        case 4: runAction([this] { serviceWarnings(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::listMaintenance() {
    TextTable t({"ID", "Vehicle", "Status", "Opened", "Closed", "Description", "Service km", "Cost"});
    t.alignRight(6);
    t.alignRight(7);
    for (const MaintenanceRecord* r : sys_.maintenance().records()) {
        const bool done = r->status() == MaintenanceStatus::Completed;
        t.addRow({r->id(), r->vehicleId(), toString(r->status()), r->openedOn().toString(),
                  done ? r->closedOn()->toString() : "-", r->description(),
                  done ? std::to_string(r->serviceOdometerKm()) : "-", done ? r->cost().toString() : "-"});
    }
    t.print(io_.out());
}

void ConsoleApp::openMaintenance() {
    io_.heading("Open maintenance");
    const std::string vehicleId = askVehicleId();
    const std::string description = io_.askText("Task description (e.g. 10,000 km service)");
    const Date opened = io_.askDateDefault("Start date", sys_.clock().today());
    const auto affected = sys_.maintenance().bookingsAffectedBy(vehicleId);
    if (!affected.empty()) {
        io_.info("Warning: these confirmed bookings cannot be collected while the vehicle is blocked:");
        for (const Booking* b : affected) io_.info("  " + b->id() + " " + b->period().toString());
    }
    io_.info("The vehicle will be BLOCKED: no quotes, bookings or pickups until maintenance is completed.");
    if (!io_.confirm("Open maintenance for " + vehicleId + "?")) throw InputCancelled{};
    const MaintenanceRecord& r = sys_.maintenance().openMaintenance(vehicleId, description, opened);
    dataChanged();
    io_.info("Opened " + r.id() + "; " + vehicleId + " is now UnderMaintenance.");
}

void ConsoleApp::completeMaintenance() {
    io_.heading("Complete maintenance");
    const std::string id = toUpper(io_.askText("Maintenance ID (e.g. MT-0001)"));
    const MaintenanceRecord& r = sys_.data().maintenance.get(id);
    const Vehicle& v = sys_.fleet().vehicle(r.vehicleId());
    const Date closed = io_.askDateDefault("Completion date", sys_.clock().today());
    const long long km = io_.askIntDefault("Odometer at service (km)", v.mileageKm(), 5000000, v.mileageKm());
    const Money cost = io_.askMoney("Cost");
    sys_.maintenance().completeMaintenance(id, closed, km, cost);
    dataChanged();
    io_.info("Completed " + id + ". " + v.id() + " is " + toString(v.status()) + ", next service due at " +
             std::to_string(v.nextServiceDueKm()) + " km.");
}

void ConsoleApp::serviceWarnings() {
    io_.info("Vehicles within " + std::to_string(sys_.data().serviceWarningKm) +
             " km of their next service. This is a WARNING only - they can still be booked.");
    TextTable t({"Vehicle", "Registration", "Status", "Odometer", "Next service", "Km remaining"});
    for (std::size_t c = 3; c <= 5; ++c) t.alignRight(c);
    for (const ServiceAlert& a : sys_.maintenance().serviceAlerts()) {
        t.addRow({a.vehicle->id(), a.vehicle->registration(), toString(a.vehicle->status()),
                  std::to_string(a.vehicle->mileageKm()), std::to_string(a.vehicle->nextServiceDueKm()),
                  a.kmRemaining < 0 ? "OVERDUE by " + std::to_string(-a.kmRemaining) : std::to_string(a.kmRemaining)});
    }
    t.print(io_.out());
}

} // namespace rental
