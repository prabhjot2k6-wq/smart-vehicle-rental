#pragma once
// RentalService: the booking life cycle.
//   confirm quote -> (extend) -> collect (pickup) -> return -> invoice
//                 -> cancel (only before pickup)
// Every step re-checks the rules at the moment it happens (availability,
// licence, quote expiry), because time passes between steps.

#include "rental/AvailabilityService.h"
#include "rental/ChargeCalculator.h"
#include "rental/Clock.h"
#include "rental/QuoteService.h"
#include "rental/RentalData.h"

#include <string>
#include <vector>

namespace rental {

class RentalService {
public:
    RentalService(RentalData& data, const AvailabilityService& availability, QuoteService& quotes,
                  const ChargeCalculator& charges, const Clock& clock)
        : data_(data), availability_(availability), quotes_(quotes), charges_(charges), clock_(clock) {}

    // Rejects expired quotes; re-checks availability and licence; honours the
    // quoted price exactly if the quote is still valid.
    const Booking& confirmQuote(const std::string& quoteId);
    void cancelBooking(const std::string& bookingId);

    PriceQuote quoteExtension(const std::string& bookingId, Date newReturnDate) const;
    const Booking& confirmExtension(const PriceQuote& extensionQuote);

    // Collection: vehicle must be Active (not under maintenance) and not still
    // out with another customer; the licence is checked again. Takes the deposit.
    const Booking& pickup(const std::string& bookingId, const InspectionReport& pickupReport);

    // Builds the invoice WITHOUT saving anything, so staff can review it.
    Invoice previewReturn(const std::string& bookingId, const InspectionReport& returnReport,
                          const std::vector<DamageCharge>& damages) const;
    // Records the return, finalizes and stores the invoice. Irreversible.
    const Invoice& completeReturn(const std::string& bookingId, const InspectionReport& returnReport,
                                  const std::vector<DamageCharge>& damages);

    const Booking& booking(const std::string& id) const { return data_.bookings.get(id); }
    std::vector<const Booking*> bookings() const { return data_.bookings.all(); }

private:
    Invoice buildInvoice(const Booking& booking, const InspectionReport& returnReport,
                         const std::vector<DamageCharge>& damages, const std::string& invoiceId) const;

    RentalData& data_;
    const AvailabilityService& availability_;
    QuoteService& quotes_;
    const ChargeCalculator& charges_;
    const Clock& clock_;
};

} // namespace rental
