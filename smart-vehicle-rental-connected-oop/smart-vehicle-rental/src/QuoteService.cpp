#include "rental/QuoteService.h"

#include <algorithm>

namespace rental {

QuoteService::QuoteService(RentalData& data, const AvailabilityService& availability, const Clock& clock,
                           std::unique_ptr<PricingStrategy> strategy)
    : data_(data), availability_(availability), clock_(clock) {
    setStrategy(std::move(strategy));
}

void QuoteService::setStrategy(std::unique_ptr<PricingStrategy> strategy) {
    if (!strategy) throw ValidationError("A pricing strategy is required.");
    strategy_ = std::move(strategy);
}

std::int64_t QuoteService::lifetimeSeconds() const {
    return static_cast<std::int64_t>(data_.pricingRules.quoteValidityMinutes()) * 60;
}

PricingContext QuoteService::buildContext(const Vehicle& vehicle, const DateRange& period,
                                          const std::string& ignoreBookingId) const {
    const VehicleModel& model = data_.models.get(vehicle.modelId());
    return PricingContext(model.displayName(), model.baseDailyRate(), period, data_.pricingRules,
                          availability_.modelDemand(model.id(), period, ignoreBookingId));
}

PriceBreakdown QuoteService::priceFor(const Vehicle& vehicle, const DateRange& period,
                                      const std::string& ignoreBookingId) const {
    return strategy_->price(buildContext(vehicle, period, ignoreBookingId));
}

std::vector<SearchResult> QuoteService::search(const SearchCriteria& c) const {
    if (c.period.start() < clock_.today()) {
        throw ValidationError("Pickup date " + c.period.start().toString() + " is in the past.");
    }
    std::vector<SearchResult> results;
    for (const Vehicle* v : data_.vehicles.all()) {
        const VehicleModel& m = data_.models.get(v->modelId());
        if (c.category && m.category() != *c.category) continue;
        if (!c.modelId.empty() && m.id() != c.modelId) continue;
        if (!availability_.isAvailable(*v, c.period)) continue;
        PriceBreakdown price = priceFor(*v, c.period);
        if (c.maxTotal && price.total() > *c.maxTotal) continue;
        results.push_back({v, &m, std::move(price)});
    }
    std::stable_sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.price.total() < b.price.total();
    });
    return results;
}

const PriceQuote& QuoteService::createQuote(const std::string& customerId, const std::string& vehicleId,
                                            const DateRange& period) {
    const Customer& customer = data_.customers.get(customerId);
    const Vehicle& vehicle = data_.vehicles.get(vehicleId);
    if (period.start() < clock_.today()) {
        throw ValidationError("Pickup date " + period.start().toString() + " is in the past.");
    }
    customer.licence().requireCovers(vehicle.category(), period);
    availability_.requireAvailable(vehicle, period);

    const std::int64_t now = clock_.nowEpochSeconds();
    std::string id = "Q-" + std::to_string(nextQuoteNumber_++);
    PriceBreakdown price = priceFor(vehicle, period);
    price.applyCustomerDiscount(customer.discountBp()); // polymorphic: depends on the customer's kind
    PriceQuote q{id, customerId, vehicleId, "", std::move(price), now, now + lifetimeSeconds()};
    return pending_.emplace(id, std::move(q)).first->second;
}

PriceQuote QuoteService::createExtensionQuote(const Booking& booking, Date newReturnDate) const {
    if (!booking.occupiesVehicle()) {
        throw ConflictError("Booking " + booking.id() + " is " + toString(booking.status()) + " and cannot be extended.");
    }
    if (newReturnDate <= booking.period().end()) {
        throw ValidationError("New return date must be after the current return date " +
                              booking.period().end().toString() + ".");
    }
    const DateRange extra(booking.period().end(), newReturnDate);
    const DateRange whole(booking.period().start(), newReturnDate);
    const Vehicle& vehicle = data_.vehicles.get(booking.vehicleId());
    data_.customers.get(booking.customerId()).licence().requireCovers(vehicle.category(), whole);
    // Ignore this booking itself when checking for clashes.
    availability_.requireAvailable(vehicle, extra, booking.id());

    const std::int64_t now = clock_.nowEpochSeconds();
    PriceBreakdown price = priceFor(vehicle, extra, booking.id());
    price.applyCustomerDiscount(data_.customers.get(booking.customerId()).discountBp());
    return PriceQuote{"EXT-" + booking.id(), booking.customerId(), booking.vehicleId(), booking.id(),
                      std::move(price), now, now + lifetimeSeconds()};
}

const PriceQuote* QuoteService::findPending(const std::string& quoteId) const {
    auto it = pending_.find(quoteId);
    return it == pending_.end() ? nullptr : &it->second;
}

std::vector<const PriceQuote*> QuoteService::pendingQuotes() const {
    std::vector<const PriceQuote*> out;
    for (const auto& entry : pending_) out.push_back(&entry.second);
    return out;
}

PriceQuote QuoteService::takePending(const std::string& quoteId) {
    auto it = pending_.find(quoteId);
    if (it == pending_.end()) throw NotFoundError("Quote " + quoteId + " not found (quotes are not kept after exit).");
    PriceQuote q = std::move(it->second);
    pending_.erase(it);
    return q;
}

std::size_t QuoteService::discardExpired() {
    const std::int64_t now = clock_.nowEpochSeconds();
    std::size_t removed = 0;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->second.isExpired(now)) {
            it = pending_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

} // namespace rental
