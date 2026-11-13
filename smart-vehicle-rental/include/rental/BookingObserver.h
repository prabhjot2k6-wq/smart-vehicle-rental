#pragma once
// OBSERVER PATTERN.
//
// RentalService is the SUBJECT: whenever a booking changes state it builds a
// BookingEvent and announces it to every registered BookingObserver. The
// service has no idea what the observers do (write a log, send an SMS, update
// a dashboard...). New behaviour is added by writing a new observer class and
// registering it - RentalService never changes (Open/Closed principle, loose
// coupling).
#include <cstdint>
#include <string>

namespace rental {

enum class BookingEventKind { Confirmed, Extended, PickedUp, Returned, Cancelled };

std::string toString(BookingEventKind kind);

struct BookingEvent {
    BookingEventKind kind = BookingEventKind::Confirmed;
    std::string bookingId;
    std::string customerId;
    std::string vehicleId;
    std::int64_t epochSeconds = 0; // when it happened (from the Clock)
    std::string detail;            // e.g. invoice number or new return date
};

// The OBSERVER interface: one pure virtual function.
class BookingObserver {
public:
    virtual ~BookingObserver() = default;
    virtual void onBookingEvent(const BookingEvent& event) = 0;
};

} // namespace rental
