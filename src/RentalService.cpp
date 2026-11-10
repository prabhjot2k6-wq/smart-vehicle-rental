#include "rental/RentalService.h"

namespace rental {

namespace {
std::string describeExpiry(std::int64_t expiresAt, std::int64_t now) {
    const std::int64_t ago = now - expiresAt;
    return "Quote expired " + std::to_string(ago / 60) + " min " + std::to_string(ago % 60) +
           " s ago. Please request a new quote (prices may have changed).";
}
} // namespace

const Booking& RentalService::confirmQuote(const std::string& quoteId) {
    // The quote is consumed whether or not confirmation succeeds.
    const PriceQuote q = quotes_.takePending(quoteId);
    const std::int64_t now = clock_.nowEpochSeconds();
    if (q.isExpired(now)) throw ValidationError(describeExpiry(q.expiresAt, now));

    // Re-check everything that could have changed since the quote was made.
    const Vehicle& vehicle = data_.vehicles.get(q.vehicleId);
    const Customer& customer = data_.customers.get(q.customerId);
    const DateRange& period = q.breakdown.period();
    if (period.start() < clock_.today()) throw ValidationError("The pickup date is now in the past.");
    customer.licence().requireCovers(vehicle.category(), period);
    availability_.requireAvailable(vehicle, period);

    // Honour the quoted price: store the breakdown exactly as quoted.
    return data_.bookings.add(Booking(data_.ids.next("B"), q.customerId, q.vehicleId, q.breakdown, now));
}

void RentalService::cancelBooking(const std::string& bookingId) {
    data_.bookings.get(bookingId).cancel();
}

PriceQuote RentalService::quoteExtension(const std::string& bookingId, Date newReturnDate) const {
    return quotes_.createExtensionQuote(data_.bookings.get(bookingId), newReturnDate);
}

const Booking& RentalService::confirmExtension(const PriceQuote& q) {
    if (q.extendsBookingId.empty()) throw ValidationError("This is not an extension quote.");
    const std::int64_t now = clock_.nowEpochSeconds();
    if (q.isExpired(now)) throw ValidationError(describeExpiry(q.expiresAt, now));

    Booking& booking = data_.bookings.get(q.extendsBookingId);
    if (booking.period().end() != q.breakdown.period().start()) {
        throw ConflictError("Booking " + booking.id() + " changed since this extension was quoted. Please re-quote.");
    }
    const Vehicle& vehicle = data_.vehicles.get(booking.vehicleId());
    const DateRange whole(booking.period().start(), q.breakdown.period().end());
    data_.customers.get(booking.customerId()).licence().requireCovers(vehicle.category(), whole);
    // Ignore the booking itself; any other overlapping booking is a conflict.
    availability_.requireAvailable(vehicle, q.breakdown.period(), booking.id());
    booking.addExtension(q.breakdown);
    return booking;
}

const Booking& RentalService::pickup(const std::string& bookingId, const InspectionReport& report) {
    Booking& booking = data_.bookings.get(bookingId);
    Vehicle& vehicle = data_.vehicles.get(booking.vehicleId());
    if (booking.status() != BookingStatus::Confirmed) {
        throw ConflictError("Booking " + bookingId + " is " + toString(booking.status()) + "; only Confirmed bookings can be collected.");
    }
    if (!vehicle.isRentable()) {
        throw ConflictError("Vehicle " + vehicle.id() + " is " + toString(vehicle.status()) +
                            " and cannot be handed over. Complete maintenance first or rebook another vehicle.");
    }
    if (const Booking* other = availability_.activeRentalFor(vehicle.id())) {
        throw ConflictError("Vehicle " + vehicle.id() + " has not been returned from booking " + other->id() + " yet.");
    }
    data_.customers.get(booking.customerId()).licence().requireCovers(vehicle.category(), booking.period());
    if (report.odometerKm() < vehicle.mileageKm()) {
        throw ValidationError("Pickup odometer " + std::to_string(report.odometerKm()) +
                              " km is lower than the vehicle's recorded " + std::to_string(vehicle.mileageKm()) + " km.");
    }
    booking.recordPickup(report, vehicle.securityDeposit()); // validates the date
    vehicle.recordOdometer(report.odometerKm());
    return booking;
}

Invoice RentalService::buildInvoice(const Booking& booking, const InspectionReport& report,
                                    const std::vector<DamageCharge>& damages, const std::string& invoiceId) const {
    booking.validateReturn(report);
    const Vehicle& vehicle = data_.vehicles.get(booking.vehicleId());
    Invoice invoice(invoiceId, booking.id(), booking.customerId(), booking.vehicleId(), report.date(),
                    booking.depositHeld());
    for (const InvoiceLine& line : charges_.rentalLines(booking)) invoice.addLine(line);
    for (const InvoiceLine& line : charges_.returnCharges(booking, vehicle.category(), report, damages)) {
        invoice.addLine(line);
    }
    return invoice;
}

Invoice RentalService::previewReturn(const std::string& bookingId, const InspectionReport& report,
                                     const std::vector<DamageCharge>& damages) const {
    return buildInvoice(data_.bookings.get(bookingId), report, damages, "DRAFT");
}

const Invoice& RentalService::completeReturn(const std::string& bookingId, const InspectionReport& report,
                                             const std::vector<DamageCharge>& damages) {
    Booking& booking = data_.bookings.get(bookingId);
    Vehicle& vehicle = data_.vehicles.get(booking.vehicleId());
    buildInvoice(booking, report, damages, "DRAFT"); // validate everything before changing anything
    if (report.odometerKm() < vehicle.mileageKm()) {
        throw ValidationError("Return odometer is lower than the vehicle's recorded mileage.");
    }
    Invoice invoice = buildInvoice(booking, report, damages, data_.ids.next("INV"));
    invoice.finalize();
    const Invoice& stored = data_.invoices.add(std::move(invoice));
    booking.recordReturn(report, stored.id());
    vehicle.recordOdometer(report.odometerKm());
    return stored;
}

} // namespace rental
