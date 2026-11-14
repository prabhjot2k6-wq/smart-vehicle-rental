#pragma once
// In-memory repositories (collections keyed by ID) and the ID generator.
//
// Repositories OWN the objects. Everyone else looks them up by ID and gets a
// reference or a non-owning pointer, which is valid as long as the repository
// lives. Relationships between objects are stored as IDs (a Booking stores
// "V-0003", not a pointer), which keeps saving/loading simple and avoids
// dangling pointers.

#include "rental/Errors.h"
#include "rental/Customer.h"
#include "rental/Vehicle.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rental {

// Orders "V-9" before "V-10" (shorter IDs first, then alphabetical).
struct IdLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return a.size() != b.size() ? a.size() < b.size() : a < b;
    }
};

// Generic repository for value-like entities that have an id() method.
template <typename T>
class Repository {
public:
    explicit Repository(std::string entityName) : entityName_(std::move(entityName)) {}

    T& add(T item) {
        const std::string id = item.id();
        if (items_.count(id) != 0) throw ConflictError(entityName_ + " ID " + id + " already exists.");
        return items_.emplace(id, std::move(item)).first->second;
    }

    T* find(const std::string& id) {
        auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }
    const T* find(const std::string& id) const {
        auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }

    T& get(const std::string& id) {
        if (T* item = find(id)) return *item;
        throw NotFoundError(entityName_ + " " + id + " not found.");
    }
    const T& get(const std::string& id) const {
        if (const T* item = find(id)) return *item;
        throw NotFoundError(entityName_ + " " + id + " not found.");
    }

    std::vector<const T*> all() const {
        std::vector<const T*> out;
        out.reserve(items_.size());
        for (const auto& entry : items_) out.push_back(&entry.second);
        return out;
    }

    std::size_t size() const { return items_.size(); }

private:
    std::string entityName_;
    std::map<std::string, T, IdLess> items_;
};

// Vehicles are polymorphic, so they are stored through std::unique_ptr.
// This repository also enforces unique registration numbers.
class VehicleRepository {
public:
    Vehicle& add(std::unique_ptr<Vehicle> vehicle);

    Vehicle* find(const std::string& id);
    const Vehicle* find(const std::string& id) const;
    Vehicle& get(const std::string& id);
    const Vehicle& get(const std::string& id) const;

    // Registration is normalised before comparing ("mh12 ab" == "MH12AB").
    const Vehicle* findByRegistration(const std::string& registration) const;

    std::vector<const Vehicle*> all() const;
    std::size_t size() const { return items_.size(); }

private:
    std::map<std::string, std::unique_ptr<Vehicle>, IdLess> items_;
};

// Customers are polymorphic too (Regular / Premium / Corporate), so they are
// stored through std::unique_ptr, exactly like vehicles.
class CustomerRepository {
public:
    Customer& add(std::unique_ptr<Customer> customer);
    void remove(const std::string& id);
    Customer* find(const std::string& id);
    const Customer* find(const std::string& id) const;
    Customer& get(const std::string& id);
    const Customer& get(const std::string& id) const;
    std::vector<const Customer*> all() const;
    std::size_t size() const { return items_.size(); }

private:
    std::map<std::string, std::unique_ptr<Customer>, IdLess> items_;
};

// Generates readable, persistent IDs: V-0001, C-0001, B-0001 ...
// Counters are saved with the data so IDs are never reused after a restart.
class IdGenerator {
public:
    std::string next(const std::string& prefix);
    // Makes sure future IDs with this prefix are larger than `id`.
    void observe(const std::string& id);
    void setCounter(const std::string& prefix, long long lastIssued);
    const std::map<std::string, long long>& counters() const { return lastIssued_; }

private:
    std::map<std::string, long long> lastIssued_;
};

} // namespace rental

