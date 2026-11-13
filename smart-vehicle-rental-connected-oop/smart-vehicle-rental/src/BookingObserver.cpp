#include "rental/BookingObserver.h"

namespace rental {

std::string toString(BookingEventKind kind) {
    switch (kind) {
    case BookingEventKind::Confirmed: return "Confirmed";
    case BookingEventKind::Extended: return "Extended";
    case BookingEventKind::PickedUp: return "PickedUp";
    case BookingEventKind::Returned: return "Returned";
    case BookingEventKind::Cancelled: return "Cancelled";
    }
    return "?";
}

} // namespace rental
