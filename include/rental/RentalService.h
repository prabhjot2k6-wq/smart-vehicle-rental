#pragma once
// RentalService: the booking life cycle.
//   confirm quote -> (extend) -> collect (pickup) -> return -> invoice
//                 -> cancel
// Every step re-checks the rules at the moment it happens (availability,
// licence, quote expiry), because time passes between steps.

#include "rental/AvailabilityService.h"
#include "rental/Clock.h"
#include "rental/QuoteService.h"
#include "rental/RentalData.h"

#include <string>

namespace rental {

class RentalService {
public:
    RentalService(RentalData& data, const AvailabilityService& availability, QuoteService& quotes,
                  const Clock& clock)
        : data_(data), availability_(availability), quotes_(quotes), clock_(clock) {}

    // Rejects expired quotes; re-checks availability and licence; honours the
    // quoted price exactly if the quote is still valid.
    const Booking& confirmQuote(const std::string& quoteId);
    void cancelBooking(const std::string& bookingId);

    PriceQuote quoteExtension(const std::string& bookingId, Date newReturnDate) const;
    const Booking& confirmExtension(const PriceQuote& extensionQuote);

    const Booking& booking(const std::string& id) const { return data_.bookings.get(id); }
    std::vector<const Booking*> bookings() const { return data_.bookings.all(); }

private:
    RentalData& data_;
    const AvailabilityService& availability_;
    QuoteService& quotes_;
    const Clock& clock_;
};

} // namespace rental
