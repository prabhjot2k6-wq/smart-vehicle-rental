#include "rental/NotificationService.h"

namespace rental {

void NotificationService::onBookingEvent(const BookingEvent& e) {
    const Customer* c = data_.customers.find(e.customerId);
    if (c == nullptr) return;

    std::string text;
    switch (e.kind) {
    case BookingEventKind::Confirmed: text = "your booking " + e.bookingId + " is confirmed."; break;
    case BookingEventKind::Extended: text = "your booking " + e.bookingId + " was extended" +
                                            (e.detail.empty() ? "." : " (" + e.detail + ")."); break;
    case BookingEventKind::PickedUp: text = "vehicle collected for booking " + e.bookingId + ". Drive safe!"; break;
    case BookingEventKind::Returned: text = "thanks for returning the vehicle (booking " + e.bookingId + ")" +
                                            (e.detail.empty() ? "." : ", " + e.detail + "."); break;
    case BookingEventKind::Cancelled: text = "your booking " + e.bookingId + " was cancelled."; break;
    }
    const std::string message = "[SMS to " + c->phone() + "] Hi " + c->name() + ", " + text;
    sent_.push_back(message);
    out_ << message << '\n';
}

} // namespace rental
