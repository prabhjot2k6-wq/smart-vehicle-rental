#include "rental/Repository.h"

#include <algorithm>
#include <cctype>

namespace rental {

Vehicle& VehicleRepository::add(std::unique_ptr<Vehicle> vehicle) {
    if (!vehicle) throw ValidationError("Cannot add an empty vehicle.");
    if (items_.count(vehicle->id()) != 0) throw ConflictError("Vehicle ID " + vehicle->id() + " already exists.");
    if (const Vehicle* other = findByRegistration(vehicle->registration())) {
        throw ConflictError("Registration " + vehicle->registration() + " is already used by vehicle " + other->id() +
                            ".");
    }
    const std::string id = vehicle->id();
    return *items_.emplace(id, std::move(vehicle)).first->second;
}

Vehicle* VehicleRepository::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

const Vehicle* VehicleRepository::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

Vehicle& VehicleRepository::get(const std::string& id) {
    if (Vehicle* v = find(id)) return *v;
    throw NotFoundError("Vehicle " + id + " not found.");
}

const Vehicle& VehicleRepository::get(const std::string& id) const {
    if (const Vehicle* v = find(id)) return *v;
    throw NotFoundError("Vehicle " + id + " not found.");
}

const Vehicle* VehicleRepository::findByRegistration(const std::string& registration) const {
    const std::string wanted = Vehicle::normalizeRegistration(registration);
    for (const auto& entry : items_) {
        if (entry.second->registration() == wanted) return entry.second.get();
    }
    return nullptr;
}

std::vector<const Vehicle*> VehicleRepository::all() const {
    std::vector<const Vehicle*> out;
    out.reserve(items_.size());
    for (const auto& entry : items_) out.push_back(entry.second.get());
    return out;
}

Customer& CustomerRepository::add(std::unique_ptr<Customer> customer) {
    if (!customer) throw ValidationError("Cannot add an empty customer.");
    const std::string id = customer->id();
    if (items_.count(id) != 0) throw ConflictError("Customer ID " + id + " already exists.");
    return *items_.emplace(id, std::move(customer)).first->second;
}

Customer* CustomerRepository::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

const Customer* CustomerRepository::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.get();
}

Customer& CustomerRepository::get(const std::string& id) {
    if (Customer* c = find(id)) return *c;
    throw NotFoundError("Customer " + id + " not found.");
}

const Customer& CustomerRepository::get(const std::string& id) const {
    if (const Customer* c = find(id)) return *c;
    throw NotFoundError("Customer " + id + " not found.");
}

std::vector<const Customer*> CustomerRepository::all() const {
    std::vector<const Customer*> out;
    out.reserve(items_.size());
    for (const auto& entry : items_) out.push_back(entry.second.get());
    return out;
}

std::string IdGenerator::next(const std::string& prefix) {
    const long long n = ++lastIssued_[prefix];
    std::string digits = std::to_string(n);
    if (digits.size() < 4) digits.insert(0, 4 - digits.size(), '0');
    return prefix + "-" + digits;
}

void IdGenerator::observe(const std::string& id) {
    const auto dash = id.rfind('-');
    if (dash == std::string::npos || dash + 1 >= id.size()) return;
    const std::string digits = id.substr(dash + 1);
    if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) return;
    if (digits.size() > 15) return;
    setCounter(id.substr(0, dash), std::stoll(digits));
}

void IdGenerator::setCounter(const std::string& prefix, long long lastIssued) {
    long long& current = lastIssued_[prefix];
    current = std::max(current, lastIssued);
}

} // namespace rental
