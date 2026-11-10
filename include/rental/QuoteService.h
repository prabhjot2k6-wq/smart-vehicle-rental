#pragma once
// QuoteService: searching availability and producing price quotes.
//
// A quote is a promise: "this vehicle, these dates, this price - valid until
// expiresAt". Pending quotes live in memory only (they expire within minutes,
// so they are not saved). RentalService turns a valid quote into a Booking.

#include "rental/AvailabilityService.h"
#include "rental/Clock.h"
#include "rental/PricingStrategy.h"
#include "rental/RentalData.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rental {

struct PriceQuote {
    std::string id;               // Q-0001 (in-memory only)
    std::string customerId;
    std::string vehicleId;
    std::string extendsBookingId; // set for extension quotes
    PriceBreakdown breakdown;
    std::int64_t createdAt;
    std::int64_t expiresAt;

    // A quote is expired from the exact expiry second onwards.
    bool isExpired(std::int64_t now) const { return now >= expiresAt; }
};

struct SearchCriteria {
    DateRange period;
    std::optional<VehicleCategory> category;
    std::string modelId;           // empty = any model
    std::optional<Money> maxTotal; // budget for the whole rental
};

struct SearchResult {
    const Vehicle* vehicle;
    const VehicleModel* model;
    PriceBreakdown price;
};

class QuoteService {
public:
    QuoteService(RentalData& data, const AvailabilityService& availability, const Clock& clock,
                 std::unique_ptr<PricingStrategy> strategy);

    const PricingStrategy& strategy() const { return *strategy_; }
    void setStrategy(std::unique_ptr<PricingStrategy> strategy);

    // Prices a period for a vehicle using a fresh, consistent snapshot.
    PriceBreakdown priceFor(const Vehicle& vehicle, const DateRange& period,
                            const std::string& ignoreBookingId = "") const;

    // Available vehicles matching the criteria, cheapest first.
    std::vector<SearchResult> search(const SearchCriteria& criteria) const;

    // Validates customer licence, dates and availability; stores a pending quote.
    const PriceQuote& createQuote(const std::string& customerId, const std::string& vehicleId,
                                  const DateRange& period);
    // Extension quotes are returned directly (the caller confirms immediately).
    PriceQuote createExtensionQuote(const Booking& booking, Date newReturnDate) const;

    const PriceQuote* findPending(const std::string& quoteId) const;
    std::vector<const PriceQuote*> pendingQuotes() const;
    // Removes and returns a pending quote (expired or not; the caller decides).
    PriceQuote takePending(const std::string& quoteId);
    std::size_t discardExpired();

    std::int64_t now() const { return clock_.nowEpochSeconds(); }
    Date today() const { return clock_.today(); }

private:
    PricingContext buildContext(const Vehicle& vehicle, const DateRange& period,
                                const std::string& ignoreBookingId) const;
    std::int64_t lifetimeSeconds() const;

    RentalData& data_;
    const AvailabilityService& availability_;
    const Clock& clock_;
    std::unique_ptr<PricingStrategy> strategy_;
    std::map<std::string, PriceQuote, IdLess> pending_;
    long long nextQuoteNumber_ = 1;
};

} // namespace rental
