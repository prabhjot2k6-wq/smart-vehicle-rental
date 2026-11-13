#pragma once
// NotificationService: a second, independent OBSERVER. It "sends" a simulated
// SMS to the customer for every booking event (it only prints the message to
// the stream it was given; nothing is really sent).
#include "rental/BookingObserver.h"
#include "rental/RentalData.h"

#include <ostream>
#include <string>
#include <vector>

namespace rental {

class NotificationService final : public BookingObserver {
public:
    NotificationService(const RentalData& data, std::ostream& out) : data_(data), out_(out) {}

    void onBookingEvent(const BookingEvent& event) override;

    const std::vector<std::string>& sent() const { return sent_; }

private:
    const RentalData& data_;
    std::ostream& out_;
    std::vector<std::string> sent_;
};

} // namespace rental
