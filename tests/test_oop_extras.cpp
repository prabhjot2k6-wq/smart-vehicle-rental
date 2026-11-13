// Tests for the OOP extensions: customer hierarchy, Observer, Factory.

#include "TestFramework.h"
#include "TestHelpers.h"

#include "rental/AuditLogger.h"
#include "rental/DataStore.h"
#include "rental/NotificationService.h"
#include "rental/VehicleTypes.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rental;
using namespace testhelp;

namespace {

InspectionReport Insp(const char* date, long long km, int fuel) { return InspectionReport(D(date), km, fuel, ""); }

DrivingLicence anyLicence(const std::string& number) {
    return DrivingLicence(number, D("2035-01-01"), {VehicleCategory::Car, VehicleCategory::Bike, VehicleCategory::Van});
}

// A tiny observer that just remembers the events it was told about.
class RecordingObserver final : public BookingObserver {
public:
    void onBookingEvent(const BookingEvent& e) override { events.push_back(e); }
    std::vector<BookingEvent> events;
};

class ThrowingObserver final : public BookingObserver {
public:
    void onBookingEvent(const BookingEvent&) override { throw std::runtime_error("observer exploded"); }
};

} // namespace

// ---------------------------------------------------------------- customers

TEST_CASE("customers: each kind answers the same questions differently (polymorphism)") {
    auto regular = CustomerFactory::create(CustomerType::Regular, "C-1", "A", "+91 90000 00001", "a@x.com", anyLicence("L1"));
    auto premium = CustomerFactory::create(CustomerType::Premium, "C-2", "B", "+91 90000 00002", "b@x.com", anyLicence("L2"));
    auto corp = CustomerFactory::create(CustomerType::Corporate, "C-3", "C", "+91 90000 00003", "c@x.com", anyLicence("L3"));

    // Always used through the base class.
    const Customer& r = *regular;
    const Customer& p = *premium;
    const Customer& c = *corp;
    CHECK_EQ(r.discountBp(), BasisPoints{0});
    CHECK_EQ(p.discountBp(), BasisPoints{500});
    CHECK_EQ(c.discountBp(), BasisPoints{1000});
    CHECK(r.maxActiveBookings() < p.maxActiveBookings());
    CHECK(p.maxActiveBookings() < c.maxActiveBookings());
    CHECK_EQ(r.typeName(), std::string("Regular"));
    CHECK_EQ(p.typeName(), std::string("Premium"));
    CHECK_EQ(c.typeName(), std::string("Corporate"));
    CHECK(dynamic_cast<const PremiumCustomer*>(&p) != nullptr);
    CHECK(dynamic_cast<const PremiumCustomer*>(&r) == nullptr);
    // Shared validation still lives in the base class.
    CHECK_THROWS_AS(CustomerFactory::create(CustomerType::Premium, "C-9", "D", "12", "d@x.com", anyLicence("L4")),
                    ValidationError);
}

TEST_CASE("customers: CustomerType text conversion round-trips") {
    for (CustomerType t : allCustomerTypes()) CHECK(parseCustomerType(toString(t)) == t);
    CHECK(parseCustomerType("premium") == CustomerType::Premium);
    CHECK_THROWS_AS(parseCustomerType("gold"), ValidationError);
}

TEST_CASE("customers: the discount of the customer's kind is added to the quote") {
    Fleet f;
    const std::string premium = f.sys.customers()
                                    .registerCustomer(CustomerType::Premium, "Priya P", "+91 90000 00011", "p@x.com",
                                                      anyLicence("PREM-1"))
                                    .id();
    const std::string corp = f.sys.customers()
                                 .registerCustomer(CustomerType::Corporate, "Acme Ltd", "+91 90000 00012", "a@x.com",
                                                   anyLicence("CORP-1"))
                                 .id();
    const DateRange tue = R("2026-11-17", "2026-11-18"); // one weekday, 0% demand -> 0.90 x 1500 = 1350
    const PriceBreakdown regularPrice = f.sys.quotes().createQuote(f.customer, f.swifts[0], tue).breakdown;
    const PriceBreakdown premiumPrice = f.sys.quotes().createQuote(premium, f.swifts[0], tue).breakdown;
    const PriceBreakdown corpPrice = f.sys.quotes().createQuote(corp, f.swifts[0], tue).breakdown;
    CHECK_EQ(regularPrice.total(), Money::fromRupees(1350));
    CHECK_EQ(premiumPrice.discountBp(), BasisPoints{500});
    CHECK_EQ(premiumPrice.total(), Money::fromRupees(1350) - Money::fromRupees(1350).applyBasisPoints(500));
    CHECK_EQ(corpPrice.total(), Money::fromRupees(1350) - Money::fromRupees(1350).applyBasisPoints(1000));
    CHECK(corpPrice.total() < premiumPrice.total());
    CHECK(premiumPrice.total() < regularPrice.total());
}

TEST_CASE("customers: the booking limit depends on the kind of customer") {
    Fleet f;
    const std::string regular = f.customer; // Regular
    const std::string premium = f.sys.customers()
                                    .registerCustomer(CustomerType::Premium, "Priya P", "+91 90000 00011", "p@x.com",
                                                      anyLicence("PREM-2"))
                                    .id();
    const int regularLimit = f.sys.customers().customer(regular).maxActiveBookings();
    const int premiumLimit = f.sys.customers().customer(premium).maxActiveBookings();
    CHECK(premiumLimit > regularLimit);

    // 8 vehicles exist; book distinct days so nothing else interferes.
    std::vector<std::string> vehicles = f.swifts;
    vehicles.insert(vehicles.end(), f.cretas.begin(), f.cretas.end());
    vehicles.insert(vehicles.end(), f.innovas.begin(), f.innovas.end());
    auto bookFor = [&](const std::string& customer, const std::string& vehicle) {
        const std::string q = f.sys.quotes().createQuote(customer, vehicle, R("2026-12-01", "2026-12-02")).id;
        return f.sys.rentals().confirmQuote(q).id();
    };
    for (int i = 0; i < regularLimit; ++i) bookFor(regular, vehicles[static_cast<std::size_t>(i)]);
    // One more is refused for the Regular customer...
    const std::string extra = f.sys.quotes()
                                  .createQuote(regular, vehicles[static_cast<std::size_t>(regularLimit)],
                                               R("2026-12-01", "2026-12-02"))
                                  .id;
    CHECK_THROWS_AS(f.sys.rentals().confirmQuote(extra), ConflictError);
    // ...but the Premium customer can still book that vehicle.
    CHECK_EQ(f.sys.rentals().booking(bookFor(premium, vehicles[static_cast<std::size_t>(regularLimit)])).status(),
             BookingStatus::Confirmed);
}

TEST_CASE("customers: kind survives save and load; old files without a kind load as Regular") {
    Fleet f;
    f.sys.customers().registerCustomer(CustomerType::Corporate, "Acme Ltd", "+91 90000 00012", "a@x.com",
                                       anyLicence("CORP-9"));
    const std::string text = DataStore::serialize(f.sys.data());
    const RentalData back = DataStore::deserialize(text);
    CHECK_EQ(back.customers.get("C-0002").typeName(), std::string("Corporate"));
    CHECK_EQ(back.customers.get("C-0001").typeName(), std::string("Regular"));
    CHECK(DataStore::serialize(back) == text);

    // Rebuild the file with the 9th field (the kind) cut off, like an older file.
    std::istringstream in(text);
    std::string line, legacy;
    while (std::getline(in, line)) {
        if (line.rfind("CUSTOMER\t", 0) == 0) line = line.substr(0, line.rfind('\t'));
        legacy += line + "\n";
    }
    const RentalData old = DataStore::deserialize(legacy);
    CHECK_EQ(old.customers.get("C-0002").typeName(), std::string("Regular"));
}

// ------------------------------------------------------------------ observer

TEST_CASE("observer: every booking step is announced to every observer") {
    Fleet f;
    auto rec = std::make_shared<RecordingObserver>();
    auto rec2 = std::make_shared<RecordingObserver>();
    f.sys.rentals().addObserver(rec);
    f.sys.rentals().addObserver(rec2);
    CHECK_EQ(f.sys.rentals().observerCount(), std::size_t{2});

    // Today is Mon 2026-11-09, so start today to allow pickup straight away.
    const std::string b = f.book(f.swifts[0], R("2026-11-09", "2026-11-10")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-09", 10000, 100));
    f.sys.rentals().completeReturn(b, Insp("2026-11-10", 10100, 100), {});
    const std::string b2 = f.book(f.swifts[1], R("2026-11-20", "2026-11-21")).id();
    f.sys.rentals().cancelBooking(b2);

    CHECK_EQ(rec->events.size(), std::size_t{5});
    CHECK(rec->events[0].kind == BookingEventKind::Confirmed);
    CHECK(rec->events[1].kind == BookingEventKind::PickedUp);
    CHECK(rec->events[2].kind == BookingEventKind::Returned);
    CHECK(rec->events[3].kind == BookingEventKind::Confirmed);
    CHECK(rec->events[4].kind == BookingEventKind::Cancelled);
    CHECK_EQ(rec->events[0].bookingId, b);
    CHECK_EQ(rec->events[0].customerId, f.customer);
    CHECK_EQ(rec->events[0].vehicleId, f.swifts[0]);
    CHECK_EQ(rec2->events.size(), rec->events.size()); // both observers were told
}

TEST_CASE("observer: a failed action is not announced, and a broken observer cannot break a booking") {
    Fleet f;
    auto rec = std::make_shared<RecordingObserver>();
    f.sys.rentals().addObserver(std::make_shared<ThrowingObserver>());
    f.sys.rentals().addObserver(rec);
    const std::string b = f.book(f.swifts[0], R("2026-11-20", "2026-11-21")).id(); // still succeeds
    CHECK_EQ(f.sys.rentals().booking(b).status(), BookingStatus::Confirmed);
    CHECK_EQ(rec->events.size(), std::size_t{1}); // the next observer was still called

    CHECK_THROWS_AS(f.sys.rentals().pickup(b, Insp("2026-11-09", 5000, 100)), ValidationError); // odometer below recorded mileage
    CHECK_EQ(rec->events.size(), std::size_t{1}); // no event for the rejected pickup
    CHECK_THROWS_AS(f.sys.rentals().addObserver(nullptr), ValidationError);
}

TEST_CASE("observer: AuditLogger keeps newest-first history and writes a log file") {
    Fleet f;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path file =
        std::filesystem::temp_directory_path() /
        ("svrms_audit_test_" + std::to_string(stamp) + ".txt");
    std::filesystem::remove(file);
    auto audit = std::make_shared<AuditLogger>(f.sys.data(), file, 3);
    f.sys.rentals().addObserver(audit);

    const std::string b = f.book(f.swifts[0], R("2026-11-09", "2026-11-10")).id();
    f.sys.rentals().pickup(b, Insp("2026-11-09", 10000, 100));
    f.sys.rentals().completeReturn(b, Insp("2026-11-10", 10100, 100), {});
    const std::string b2 = f.book(f.swifts[1], R("2026-11-20", "2026-11-21")).id();
    f.sys.rentals().cancelBooking(b2);

    CHECK_EQ(audit->count(), std::size_t{3}); // only the newest 3 kept in memory
    const auto recent = audit->recent(10);
    CHECK_EQ(recent.size(), std::size_t{3});
    CHECK_EQ(recent[0].title, std::string("Booking cancelled")); // newest first
    CHECK(recent[0].text.find(b2) != std::string::npos);
    CHECK(recent[0].text.find("Test Customer") != std::string::npos); // ID turned into a name

    std::ifstream in(file);
    std::string line;
    int lines = 0;
    while (std::getline(in, line)) ++lines;
    CHECK_EQ(lines, 5); // the file keeps everything
    in.close();
    std::filesystem::remove(file);
}

TEST_CASE("observer: NotificationService sends one simulated SMS per event") {
    Fleet f;
    std::ostringstream out;
    auto notes = std::make_shared<NotificationService>(f.sys.data(), out);
    f.sys.rentals().addObserver(notes);
    const std::string b = f.book(f.swifts[0], R("2026-11-20", "2026-11-21")).id();
    f.sys.rentals().cancelBooking(b);
    CHECK_EQ(notes->sent().size(), std::size_t{2});
    CHECK(notes->sent()[0].find("+91 90000 00000") != std::string::npos);
    CHECK(notes->sent()[0].find("confirmed") != std::string::npos);
    CHECK(notes->sent()[1].find("cancelled") != std::string::npos);
    CHECK(out.str().find("[SMS to") != std::string::npos);
}

// ------------------------------------------------------------------- factory

TEST_CASE("factory: VehicleFactory builds the right type for each category") {
    VehicleData d;
    d.id = "V-1";
    d.registration = "MH12AB1234";
    d.modelId = "M-1";
    const VehicleFactory& factory = VehicleFactory::instance();
    CHECK(factory.knows(VehicleCategory::Car));
    CHECK(dynamic_cast<Car*>(factory.create(VehicleCategory::Car, d, 5).get()) != nullptr);
    CHECK(dynamic_cast<Bike*>(factory.create(VehicleCategory::Bike, d, 350).get()) != nullptr);
    CHECK(dynamic_cast<Van*>(factory.create(VehicleCategory::Van, d, 800).get()) != nullptr);
    CHECK_THROWS_AS(factory.create(VehicleCategory::Car, d, 1), ValidationError); // type validates its own spec
}

TEST_CASE("factory: a new creator can be registered without editing existing classes") {
    VehicleData d;
    d.id = "V-1";
    d.registration = "MH12AB1234";
    d.modelId = "M-1";
    int calls = 0;
    VehicleFactory& factory = VehicleFactory::instance();
    // Replace the Van creator with a counting one that still builds a Van.
    factory.registerCreator(VehicleCategory::Van, [&calls](const VehicleData& data, int spec) {
        ++calls;
        return std::make_unique<Van>(data, spec);
    });
    CHECK(factory.create(VehicleCategory::Van, d, 800) != nullptr);
    CHECK_EQ(calls, 1);
    // Put the normal creator back so other tests are unaffected.
    factory.registerCreator(VehicleCategory::Van,
                            [](const VehicleData& data, int spec) { return std::make_unique<Van>(data, spec); });
    CHECK_THROWS_AS(factory.registerCreator(VehicleCategory::Van, nullptr), ValidationError);
}



