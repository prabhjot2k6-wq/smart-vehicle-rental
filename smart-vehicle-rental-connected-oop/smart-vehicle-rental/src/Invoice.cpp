#include "rental/Invoice.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

namespace rental {

namespace {
const char* const kChargeNames[] = {"Rental", "Extension", "DurationDiscount", "LateReturn",
                                    "ExcessMileage", "Fuel", "Damage"};
} // namespace

std::string toString(ChargeType type) { return kChargeNames[static_cast<int>(type)]; }

ChargeType parseChargeType(const std::string& text) {
    for (int i = 0; i < 7; ++i) {
        if (text == kChargeNames[i]) return static_cast<ChargeType>(i);
    }
    throw ValidationError("Unknown charge type '" + text + "'.");
}

Invoice::Invoice(std::string id, std::string bookingId, std::string customerId, std::string vehicleId,
                 Date issueDate, Money depositHeld)
    : id_(requireText(id, "Invoice ID")),
      bookingId_(requireText(bookingId, "Booking ID")),
      customerId_(requireText(customerId, "Customer ID")),
      vehicleId_(requireText(vehicleId, "Vehicle ID")),
      issueDate_(issueDate),
      depositHeld_(depositHeld) {
    if (depositHeld_ < Money()) throw ValidationError("Deposit cannot be negative.");
}

Money Invoice::totalCharges() const {
    Money sum;
    for (const InvoiceLine& l : lines_) sum += l.amount;
    return sum;
}

Money Invoice::rentalCharges() const {
    Money sum;
    for (const InvoiceLine& l : lines_) {
        if (l.type == ChargeType::Rental || l.type == ChargeType::Extension || l.type == ChargeType::DurationDiscount) {
            sum += l.amount;
        }
    }
    return sum;
}

Money Invoice::additionalCharges() const { return totalCharges() - rentalCharges(); }

void Invoice::addLine(const InvoiceLine& line) {
    if (status_ != InvoiceStatus::Draft) throw ConflictError("Invoice " + id_ + " is finalized and cannot change.");
    if (trim(line.description).empty()) throw ValidationError("Invoice line needs a description.");
    if (line.type == ChargeType::DurationDiscount ? line.amount > Money() : line.amount < Money()) {
        throw ValidationError("Only discounts may be negative on an invoice.");
    }
    lines_.push_back(line);
}

void Invoice::finalize() {
    if (status_ != InvoiceStatus::Draft) throw ConflictError("Invoice " + id_ + " is already finalized.");
    status_ = InvoiceStatus::Finalized;
}

} // namespace rental
