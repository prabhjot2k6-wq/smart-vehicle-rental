// Main loop plus the vehicle and customer menus.
#include "ConsoleApp.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <iostream>
#include <sstream>

namespace rental {

void ConsoleApp::run() {
    io_.info("Smart Vehicle Rental Management System (educational demo - synthetic data, simulated payments)");
    io_.info("Tip: type 'cancel' at any prompt to go back.");
    while (true) {
        int choice = 0;
        try {
            choice = io_.menu("Main menu",
                              {"Vehicle management", "Customer management", "Availability search and price quotes",
                               "Booking management", "Pickup and return", "Maintenance", "Reports"},
                              "Exit");
        } catch (const InputClosed&) {
            choice = 0;
        }
        try {
            switch (choice) {
            case 1: vehicleMenu(); break;
            case 2: customerMenu(); break;
            case 3: quoteMenu(); break;
            case 4: bookingMenu(); break;
            case 0: io_.info("Goodbye."); return;
            default: io_.info("This menu arrives in a later milestone."); break;
            }
        } catch (const InputClosed&) {
            io_.info("Input closed. Goodbye.");
            return;
        }
    }
}

void ConsoleApp::runAction(const std::function<void()>& action) {
    try {
        action();
    } catch (const InputCancelled&) {
        io_.info("Cancelled - nothing was changed.");
    } catch (const RentalError& e) {
        io_.error(e.what());
    }
}

void ConsoleApp::dataChanged() {}

// ===================================================================== pickers
VehicleCategory ConsoleApp::askCategory() {
    std::vector<std::string> labels;
    for (VehicleCategory c : allVehicleCategories()) labels.push_back(toString(c));
    return allVehicleCategories()[io_.choose("Category", labels)];
}

FuelType ConsoleApp::askFuelType(FuelType current, bool hasDefault) {
    std::vector<std::string> labels;
    for (FuelType f : allFuelTypes()) labels.push_back(toString(f));
    if (!hasDefault) return allFuelTypes()[io_.choose("Fuel type", labels)];
    for (std::size_t i = 0; i < labels.size(); ++i) io_.out() << "  " << (i + 1) << ". " << labels[i] << "\n";
    const long long idx = io_.askIntDefault("Fuel type", 1, static_cast<long long>(labels.size()),
                                            static_cast<long long>(static_cast<int>(current)) + 1);
    return allFuelTypes()[static_cast<std::size_t>(idx - 1)];
}

std::string ConsoleApp::pickModelId() {
    const auto models = sys_.fleet().models();
    if (models.empty()) throw ValidationError("No vehicle models yet - add a model first.");
    std::vector<std::string> labels;
    for (const VehicleModel* m : models) {
        labels.push_back(m->id() + "  " + m->displayName() + " (" + toString(m->category()) + ", " +
                         m->baseDailyRate().toString() + "/day)");
    }
    return models[io_.choose("Model", labels)]->id();
}

std::string ConsoleApp::askVehicleId() {
    const std::string id = toUpper(io_.askText("Vehicle ID (e.g. V-0001)"));
    sys_.fleet().vehicle(id); // throws NotFoundError with a clear message
    return id;
}

std::string ConsoleApp::askCustomerId() {
    const std::string id = toUpper(io_.askText("Customer ID (e.g. C-0001)"));
    sys_.customers().customer(id);
    return id;
}

// ===================================================================== vehicles
void ConsoleApp::vehicleMenu() {
    while (true) {
        const int c = io_.menu("Vehicle management",
                               {"List vehicles", "List vehicle models", "Add vehicle model", "Edit model / base rate",
                                "Add vehicle", "Update vehicle", "Deactivate vehicle", "Reactivate vehicle"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { listVehicles(); }); break;
        case 2: runAction([this] { listModels(); }); break;
        case 3: runAction([this] { addModel(); }); break;
        case 4: runAction([this] { editModel(); }); break;
        case 5: runAction([this] { addVehicle(); }); break;
        case 6: runAction([this] { updateVehicle(); }); break;
        case 7: runAction([this] { deactivateVehicle(); }); break;
        case 8: runAction([this] { reactivateVehicle(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::listModels() {
    TextTable t({"Model ID", "Brand", "Model", "Category", "Base rate/day", "Vehicles"});
    t.alignRight(4);
    t.alignRight(5);
    for (const VehicleModel* m : sys_.fleet().models()) {
        int count = 0;
        for (const Vehicle* v : sys_.fleet().vehicles()) count += v->modelId() == m->id() ? 1 : 0;
        t.addRow({m->id(), m->brand(), m->name(), toString(m->category()), m->baseDailyRate().toString(),
                  std::to_string(count)});
    }
    t.print(io_.out());
}

void ConsoleApp::addModel() {
    io_.heading("Add vehicle model");
    const std::string brand = io_.askText("Brand (e.g. Maruti)");
    const std::string name = io_.askText("Model name (e.g. Swift)");
    const VehicleCategory cat = askCategory();
    const Money rate = io_.askMoney("Base daily rate");
    const VehicleModel& m = sys_.fleet().addModel(brand, name, cat, rate);
    dataChanged();
    io_.info("Added model " + m.id() + ": " + m.displayName() + " at " + m.baseDailyRate().toString() + "/day.");
}

void ConsoleApp::editModel() {
    const std::string id = pickModelId();
    const VehicleModel& m = sys_.data().models.get(id);
    const std::string brand = io_.askTextDefault("Brand", m.brand());
    const std::string name = io_.askTextDefault("Model name", m.name());
    const Money rate = io_.askMoneyDefault("Base daily rate", m.baseDailyRate());
    if (rate != m.baseDailyRate()) {
        io_.info("Note: the new rate applies to new quotes only. Confirmed bookings keep their agreed price.");
        if (!io_.confirm("Change base rate from " + m.baseDailyRate().toString() + " to " + rate.toString() + "?")) {
            throw InputCancelled{};
        }
    }
    sys_.fleet().renameModel(id, brand, name);
    sys_.fleet().setModelRate(id, rate);
    dataChanged();
    io_.info("Model " + id + " updated.");
}

void ConsoleApp::listVehicles() {
    TextTable t({"ID", "Registration", "Model", "Type", "Fuel", "Odometer km", "Next service km", "Status", "Spec"});
    t.alignRight(5);
    t.alignRight(6);
    for (const Vehicle* v : sys_.fleet().vehicles()) {
        t.addRow({v->id(), v->registration(), sys_.fleet().modelOf(*v).displayName(), toString(v->category()),
                  toString(v->fuelType()), std::to_string(v->mileageKm()), std::to_string(v->nextServiceDueKm()),
                  toString(v->status()), v->specSummary()});
    }
    t.print(io_.out());
}

void ConsoleApp::addVehicle() {
    io_.heading("Add vehicle");
    const std::string modelId = pickModelId();
    const VehicleModel& model = sys_.data().models.get(modelId);
    const std::string reg = io_.askText("Registration number (e.g. MH12 AB 1234)");
    const FuelType fuel = askFuelType(FuelType::Petrol, false);
    const long long km = io_.askInt("Current odometer (km)", 0, 2000000);
    const char* specPrompt = model.category() == VehicleCategory::Car    ? "Seats (2-9)"
                             : model.category() == VehicleCategory::Bike ? "Engine size cc (50-2000)"
                                                                         : "Payload kg (100-5000)";
    const int spec = static_cast<int>(io_.askInt(specPrompt, 0, 100000));
    const Vehicle& v = sys_.fleet().addVehicle(modelId, reg, fuel, km, spec);
    dataChanged();
    io_.info("Added " + toString(v.category()) + " " + v.id() + " (" + v.registration() + "), next service due at " +
             std::to_string(v.nextServiceDueKm()) + " km.");
}

void ConsoleApp::updateVehicle() {
    const std::string id = askVehicleId();
    const Vehicle& v = sys_.fleet().vehicle(id);
    io_.info("Press Enter to keep the current value.");
    const std::string reg = io_.askTextDefault("Registration", v.registration());
    const FuelType fuel = askFuelType(v.fuelType(), true);
    const int spec = static_cast<int>(io_.askIntDefault(v.specLabel(), 0, 100000, v.specValue()));
    const long long nextService = io_.askIntDefault("Next service due at (km)", 1, 5000000, v.nextServiceDueKm());
    // Validate everything before changing anything.
    if (const Vehicle* other = sys_.data().vehicles.findByRegistration(reg)) {
        if (other != &v) throw ConflictError("Registration " + other->registration() + " is used by " + other->id() + ".");
    }
    sys_.fleet().updateSpec(id, spec);
    sys_.fleet().updateRegistration(id, reg);
    sys_.fleet().updateFuelType(id, fuel);
    sys_.fleet().updateNextServiceDue(id, nextService);
    dataChanged();
    io_.info("Vehicle " + id + " updated.");
}

void ConsoleApp::deactivateVehicle() {
    const std::string id = askVehicleId();
    io_.info("Deactivated vehicles cannot be booked, but their bookings and invoices are kept.");
    if (!io_.confirm("Deactivate " + id + "?")) throw InputCancelled{};
    sys_.fleet().deactivateVehicle(id);
    dataChanged();
    io_.info("Vehicle " + id + " deactivated.");
}

void ConsoleApp::reactivateVehicle() {
    const std::string id = askVehicleId();
    sys_.fleet().reactivateVehicle(id);
    dataChanged();
    io_.info("Vehicle " + id + " is active again.");
}

// ===================================================================== customers
void ConsoleApp::customerMenu() {
    while (true) {
        const int c = io_.menu("Customer management", {"List customers", "Register customer", "Update customer"});
        switch (c) {
        case 0: return;
        case 1: runAction([this] { listCustomers(); }); break;
        case 2: runAction([this] { registerCustomer(); }); break;
        case 3: runAction([this] { updateCustomer(); }); break;
        default: break;
        }
    }
}

void ConsoleApp::listCustomers() {
    TextTable t({"ID", "Name", "Phone", "Email", "Licence", "Expires", "Categories"});
    for (const Customer* c : sys_.customers().customers()) {
        t.addRow({c->id(), c->name(), c->phone(), c->email(), c->licence().number(), c->licence().expiry().toString(),
                  c->licence().categoriesText()});
    }
    t.print(io_.out());
}

DrivingLicence ConsoleApp::askLicence(const DrivingLicence* current) {
    while (true) {
        const std::string number = current ? io_.askTextDefault("Licence number (synthetic)", current->number())
                                           : io_.askText("Licence number (synthetic)");
        const Date expiry = current ? io_.askDateDefault("Licence expiry", current->expiry())
                                    : io_.askDate("Licence expiry");
        const std::string catText =
            current ? io_.askTextDefault("Permitted categories, comma separated (Car,Bike,Van)", current->categoriesText())
                    : io_.askText("Permitted categories, comma separated (Car,Bike,Van)");
        try {
            std::set<VehicleCategory> cats;
            std::stringstream ss(catText);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!trim(token).empty()) cats.insert(parseVehicleCategory(trim(token)));
            }
            return DrivingLicence(number, expiry, cats);
        } catch (const ValidationError& e) {
            io_.error(e.what());
        }
    }
}

void ConsoleApp::registerCustomer() {
    io_.heading("Register customer (use synthetic data only)");
    const std::string name = io_.askText("Full name");
    const std::string phone = io_.askText("Phone");
    const std::string email = io_.askText("Email");
    const DrivingLicence lic = askLicence(nullptr);
    const Customer& c = sys_.customers().registerCustomer(name, phone, email, lic);
    dataChanged();
    io_.info("Registered " + c.id() + ": " + c.name() + ".");
}

void ConsoleApp::updateCustomer() {
    const std::string id = askCustomerId();
    const Customer& c = sys_.customers().customer(id);
    io_.info("Press Enter to keep the current value.");
    const std::string name = io_.askTextDefault("Full name", c.name());
    const std::string phone = io_.askTextDefault("Phone", c.phone());
    const std::string email = io_.askTextDefault("Email", c.email());
    const DrivingLicence lic = askLicence(&c.licence());
    // Validate contact first using a throw-away copy, then apply both updates.
    Customer probe(c.id(), name, phone, email, lic);
    sys_.customers().updateLicence(id, lic);
    sys_.customers().updateContact(id, name, phone, email);
    dataChanged();
    io_.info("Customer " + id + " updated.");
}

} // namespace rental
