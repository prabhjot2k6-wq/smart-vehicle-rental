#pragma once
// Invoice = header + itemised lines (COMPOSITION: the lines belong to the
// invoice). The refundable deposit is kept SEPARATE from charges: it is not
// revenue and is never added to the invoice total.
//
// Draft invoices can still receive lines; finalize() locks them. Only
// finalized invoices count towards revenue.

#include "rental/Date.h"
#include "rental/Enums.h"
#include "rental/Money.h"

#include <string>
#include <vector>

namespace rental {

enum class ChargeType { Rental, Extension, DurationDiscount, LateReturn, ExcessMileage, Fuel, Damage };

std::string toString(ChargeType type);
ChargeType parseChargeType(const std::string& text);

struct InvoiceLine {
    ChargeType type;
    std::string description;
    Money amount; // negative only for discounts
};

class Invoice {
public:
    Invoice(std::string id, std::string bookingId, std::string customerId, std::string vehicleId, Date issueDate,
            Money depositHeld);

    const std::string& id() const { return id_; }
    const std::string& bookingId() const { return bookingId_; }
    const std::string& customerId() const { return customerId_; }
    const std::string& vehicleId() const { return vehicleId_; }
    Date issueDate() const { return issueDate_; }
    InvoiceStatus status() const { return status_; }
    const std::vector<InvoiceLine>& lines() const { return lines_; }

    Money totalCharges() const;      // sum of all lines
    Money rentalCharges() const;     // rental + extensions - discounts
    Money additionalCharges() const; // late, mileage, fuel, damage
    Money depositHeld() const { return depositHeld_; }
    // Simulated settlement: the deposit is refunded in full and charges are paid separately.
    Money depositRefunded() const { return depositHeld_; }

    void addLine(const InvoiceLine& line); // Draft only
    void finalize();                       // Draft -> Finalized

private:
    std::string id_;
    std::string bookingId_;
    std::string customerId_;
    std::string vehicleId_;
    Date issueDate_;
    Money depositHeld_;
    InvoiceStatus status_ = InvoiceStatus::Draft;
    std::vector<InvoiceLine> lines_;
};

} // namespace rental
