#pragma once
// Error hierarchy for the rental system.
//
// Every business-rule failure throws one of these. The console layer catches
// RentalError and prints e.what(), so menus never need to know the rules.
// Inheritance is meaningful here: callers can catch the general RentalError
// or a specific kind (e.g. tests check that a clash throws ConflictError).

#include <stdexcept>
#include <string>

namespace rental {

class RentalError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Input breaks a rule (bad date, negative amount, licence does not cover category...).
class ValidationError : public RentalError {
public:
    using RentalError::RentalError;
};

// A referenced record (vehicle, customer, booking...) does not exist.
class NotFoundError : public RentalError {
public:
    using RentalError::RentalError;
};

// The request clashes with existing state (overlapping booking, duplicate ID,
// illegal status transition such as cancelling a completed booking).
class ConflictError : public RentalError {
public:
    using RentalError::RentalError;
};

// The data file is missing pieces, malformed or internally inconsistent,
// or could not be written.
class PersistenceError : public RentalError {
public:
    using RentalError::RentalError;
};

} // namespace rental
