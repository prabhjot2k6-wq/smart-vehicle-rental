#include "rental/CustomerService.h"

namespace rental {

void CustomerService::requireUniqueLicence(const std::string& licenceNumber,
                                           const std::string& exceptCustomerId) const {
    for (const Customer* c : data_.customers.all()) {
        if (c->id() != exceptCustomerId && c->licence().number() == licenceNumber) {
            throw ConflictError("Licence " + licenceNumber + " is already registered to customer " + c->id() + ".");
        }
    }
}

const Customer& CustomerService::registerCustomer(const std::string& name, const std::string& phone,
                                                  const std::string& email, const DrivingLicence& licence) {
    return registerCustomer(CustomerType::Regular, name, phone, email, licence);
}

const Customer& CustomerService::registerCustomer(CustomerType type, const std::string& name, const std::string& phone,
                                                  const std::string& email, const DrivingLicence& licence) {
    requireUniqueLicence(licence.number(), "");
    // Build a probe first: it validates name/phone/email before an ID is used up.
    auto probe = CustomerFactory::create(type, "pending", name, phone, email, licence);
    return data_.customers.add(CustomerFactory::create(type, data_.ids.next("C"), probe->name(), probe->phone(),
                                                       probe->email(), probe->licence()));
}

void CustomerService::removeCustomer(const std::string& customerId) {
    data_.customers.get(customerId); // Validate that the customer exists.
    for (const Booking* booking : data_.bookings.all()) {
        if (booking->customerId() == customerId) {
            throw ConflictError("Customer " + customerId + " has booking history and cannot be removed.");
        }
    }
    data_.customers.remove(customerId);
}
void CustomerService::updateDetails(const std::string& customerId, const std::string& name,
                                    const std::string& phone, const std::string& email,
                                    const DrivingLicence& licence) {
    Customer& customer = data_.customers.get(customerId);
    requireUniqueLicence(licence.number(), customerId);
    auto validated = CustomerFactory::create(customer.type(), customerId, name, phone, email, licence);
    customer.updateContact(validated->name(), validated->phone(), validated->email());
    customer.replaceLicence(validated->licence());
}
void CustomerService::updateContact(const std::string& customerId, const std::string& name,
                                    const std::string& phone, const std::string& email) {
    data_.customers.get(customerId).updateContact(name, phone, email);
}

void CustomerService::updateLicence(const std::string& customerId, const DrivingLicence& licence) {
    Customer& c = data_.customers.get(customerId);
    requireUniqueLicence(licence.number(), customerId);
    c.replaceLicence(licence);
}

} // namespace rental


