#include "rental/DataStore.h"

#include "rental/Errors.h"
#include "rental/VehicleTypes.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace rental {

// ============================================================ escaping

std::string DataStore::escapeField(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
        }
    }
    return out;
}

std::string DataStore::unescapeField(const std::string& escaped) {
    std::string out;
    out.reserve(escaped.size());
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] != '\\') {
            out += escaped[i];
            continue;
        }
        if (++i == escaped.size()) throw PersistenceError("dangling escape character '\\' at end of field");
        switch (escaped[i]) {
        case '\\': out += '\\'; break;
        case 't': out += '\t'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        default: throw PersistenceError(std::string("unknown escape sequence '\\") + escaped[i] + "'");
        }
    }
    return out;
}

std::vector<std::string> DataStore::splitRecord(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        fields.push_back(unescapeField(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start)));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return fields;
}

// ============================================================ writing

namespace {

class RecordWriter {
public:
    template <typename... Fields>
    void write(const std::string& type, const Fields&... fields) {
        out_ << type;
        (writeField(fields), ...);
        out_ << '\n';
        ++count_;
    }
    std::string finish() {
        out_ << "END\t" << count_ << '\n';
        return out_.str();
    }
    void header() { out_ << "SVRMS-DATA\t" << DataStore::kFormatVersion << '\n'; }

private:
    void writeField(const std::string& s) { out_ << '\t' << DataStore::escapeField(s); }
    void writeField(const char* s) { writeField(std::string(s)); }
    void writeField(long long v) { out_ << '\t' << v; }
    void writeField(int v) { out_ << '\t' << v; }
    void writeField(Money m) { out_ << '\t' << m.paise(); }
    void writeField(Date d) { out_ << '\t' << d.toString(); }
    void writeField(bool b) { out_ << '\t' << (b ? 1 : 0); }

    std::ostringstream out_;
    long long count_ = 0;
};

std::string categoriesField(const std::set<VehicleCategory>& cats) {
    std::string s;
    for (VehicleCategory c : cats) s += (s.empty() ? "" : ",") + toString(c);
    return s;
}

void writePrice(RecordWriter& w, const std::string& bookingId, int seq, const PriceBreakdown& p) {
    w.write("PRICE", bookingId, seq, p.period().start(), p.period().end(), p.strategyName(), p.baseDailyRate(),
            static_cast<long long>(p.discountBp()), p.subtotal(), p.total());
    for (const DailyPrice& d : p.days()) {
        w.write("PDAY", bookingId, seq, d.date, d.booked, d.capacity, static_cast<long long>(d.demandFactorBp),
                d.demandReason, static_cast<long long>(d.calendarFactorBp), d.calendarReason,
                static_cast<long long>(d.appliedFactorBp), d.capped, d.amount);
    }
}

void writeInspection(RecordWriter& w, const std::string& bookingId, const char* kind, const InspectionReport& r) {
    w.write("INSPECT", bookingId, kind, r.date(), r.odometerKm(), r.fuelPercent(), r.notes());
}

} // namespace

std::string DataStore::serialize(const RentalData& data) {
    RecordWriter w;
    w.header();

    for (const auto& entry : data.ids.counters()) w.write("COUNTER", entry.first, entry.second);

    const PricingRules& pr = data.pricingRules;
    w.write("PRULE", "weekend", static_cast<long long>(pr.weekendFactorBp()));
    w.write("PRULE", "holiday", static_cast<long long>(pr.holidayFactorBp()));
    w.write("PRULE", "cap", static_cast<long long>(pr.maxDailyFactorBp()));
    w.write("PRULE", "quote_minutes", static_cast<long long>(pr.quoteValidityMinutes()));
    for (const DemandTier& t : pr.demandTiers()) {
        w.write("PTIER", static_cast<long long>(t.minUtilizationBp), static_cast<long long>(t.factorBp));
    }
    for (const DurationDiscount& d : pr.durationDiscounts()) {
        w.write("PDISC", d.minDays, static_cast<long long>(d.discountBp));
    }
    for (Date h : pr.holidays()) w.write("HOLIDAY", h);

    w.write("CRULE", "late_multiplier", static_cast<long long>(data.chargeRules.lateDayMultiplierBp()));
    for (VehicleCategory c : allVehicleCategories()) {
        const CategoryCharges& cc = data.chargeRules.forCategory(c);
        w.write("CCAT", toString(c), cc.freeKmPerDay, cc.excessPerKm, cc.fuelPerPercentPoint);
    }
    w.write("SETTING", "service_warning_km", data.serviceWarningKm);

    for (const VehicleModel* m : data.models.all()) {
        w.write("MODEL", m->id(), m->brand(), m->name(), toString(m->category()), m->baseDailyRate());
    }
    for (const Vehicle* v : data.vehicles.all()) {
        w.write("VEHICLE", v->id(), toString(v->category()), v->registration(), v->modelId(), toString(v->fuelType()),
                v->mileageKm(), toString(v->status()), v->nextServiceDueKm(), v->specValue());
    }
    for (const Customer* c : data.customers.all()) {
        w.write("CUSTOMER", c->id(), c->name(), c->phone(), c->email(), c->licence().number(), c->licence().expiry(),
                categoriesField(c->licence().permittedCategories()), toString(c->type()));
    }
    for (const Booking* b : data.bookings.all()) {
        w.write("BOOKING", b->id(), b->customerId(), b->vehicleId(), toString(b->status()),
                static_cast<long long>(b->createdAtEpoch()), b->depositHeld(), b->invoiceId());
        for (std::size_t i = 0; i < b->priceParts().size(); ++i) {
            writePrice(w, b->id(), static_cast<int>(i), b->priceParts()[i]);
        }
        if (b->pickupReport()) writeInspection(w, b->id(), "PICKUP", *b->pickupReport());
        if (b->returnReport()) writeInspection(w, b->id(), "RETURN", *b->returnReport());
    }
    for (const Invoice* inv : data.invoices.all()) {
        w.write("INVOICE", inv->id(), inv->bookingId(), inv->customerId(), inv->vehicleId(), inv->issueDate(),
                inv->depositHeld(), toString(inv->status()));
        for (const InvoiceLine& l : inv->lines()) w.write("INVLINE", inv->id(), toString(l.type), l.description, l.amount);
    }
    for (const MaintenanceRecord* r : data.maintenance.all()) {
        const bool done = r->status() == MaintenanceStatus::Completed;
        w.write("MAINT", r->id(), r->vehicleId(), toString(r->status()), r->description(), r->openedOn(),
                r->odometerAtOpenKm(), done ? r->closedOn()->toString() : std::string(), r->serviceOdometerKm(),
                r->cost());
    }
    return w.finish();
}

// ============================================================ reading

namespace {

[[noreturn]] void fail(std::size_t line, const std::string& message) {
    throw PersistenceError("Data file line " + std::to_string(line) + ": " + message);
}

struct Row {
    std::size_t line = 0;
    std::vector<std::string> f; // f[0] is the record type

    void expectFields(std::size_t n) const {
        if (f.size() != n) {
            fail(line, f[0] + " record needs " + std::to_string(n - 1) + " fields but has " +
                           std::to_string(f.size() - 1));
        }
    }
    const std::string& s(std::size_t i) const { return f.at(i); }
    long long num(std::size_t i) const {
        const std::string& t = f.at(i);
        const bool neg = !t.empty() && t[0] == '-';
        const bool digits = t.size() > (neg ? 1u : 0u) && t.size() <= 19 &&
                            std::all_of(t.begin() + (neg ? 1 : 0), t.end(),
                                        [](unsigned char c) { return std::isdigit(c) != 0; });
        if (!digits) fail(line, "field " + std::to_string(i) + " should be a whole number but is '" + t + "'");
        return std::stoll(t);
    }
    Money money(std::size_t i) const { return Money::fromPaise(num(i)); }
    Date date(std::size_t i) const {
        try {
            return Date::parse(f.at(i));
        } catch (const ValidationError& e) {
            fail(line, e.what());
        }
    }
};

using RowsByType = std::map<std::string, std::vector<Row>>;

// Runs `body`, turning any domain error into a PersistenceError naming the line.
template <typename Body>
auto atLine(std::size_t line, Body body) -> decltype(body()) {
    try {
        return body();
    } catch (const PersistenceError&) {
        throw;
    } catch (const RentalError& e) {
        fail(line, e.what());
    }
}

RowsByType parseLines(const std::string& content) {
    static const std::set<std::string> kKnown = {"COUNTER", "PRULE", "PTIER",  "PDISC",   "HOLIDAY", "CRULE",
                                                 "CCAT",    "SETTING", "MODEL", "VEHICLE", "CUSTOMER", "BOOKING",
                                                 "PRICE",   "PDAY",  "INSPECT", "INVOICE", "INVLINE", "MAINT"};
    std::istringstream in(content);
    std::string text;
    std::size_t lineNo = 0;
    RowsByType rows;
    long long records = 0;
    bool sawEnd = false;

    while (std::getline(in, text)) {
        ++lineNo;
        if (!text.empty() && text.back() == '\r') text.pop_back(); // tolerate CRLF
        if (sawEnd) {
            if (!text.empty()) fail(lineNo, "unexpected content after END record");
            continue;
        }
        if (lineNo == 1) {
            std::vector<std::string> h;
            try {
                h = DataStore::splitRecord(text);
            } catch (const PersistenceError& e) {
                fail(1, e.what());
            }
            if (h.size() != 2 || h[0] != "SVRMS-DATA") fail(1, "not a Smart Vehicle Rental data file (bad header)");
            if (h[1] != std::to_string(DataStore::kFormatVersion)) {
                fail(1, "unsupported format version '" + h[1] + "' (this program reads version " +
                            std::to_string(DataStore::kFormatVersion) + ")");
            }
            continue;
        }
        if (text.empty()) fail(lineNo, "unexpected empty line");
        Row row;
        row.line = lineNo;
        try {
            row.f = DataStore::splitRecord(text);
        } catch (const PersistenceError& e) {
            fail(lineNo, e.what());
        }
        if (row.f[0] == "END") {
            row.expectFields(2);
            if (row.num(1) != records) {
                fail(lineNo, "END says " + row.s(1) + " records but " + std::to_string(records) + " were read");
            }
            sawEnd = true;
            continue;
        }
        if (kKnown.count(row.f[0]) == 0) fail(lineNo, "unknown record type '" + row.f[0] + "'");
        ++records;
        rows[row.f[0]].push_back(std::move(row));
    }
    if (lineNo == 0) throw PersistenceError("Data file is empty.");
    if (!sawEnd) throw PersistenceError("Data file is incomplete: END record missing (file may be truncated).");
    return rows;
}

const std::vector<Row>& rowsOf(const RowsByType& rows, const char* type) {
    static const std::vector<Row> kNone;
    auto it = rows.find(type);
    return it == rows.end() ? kNone : it->second;
}

void loadRules(const RowsByType& rows, RentalData& data) {
    PricingRules pr = PricingRules::defaults();
    std::vector<DemandTier> tiers;
    std::vector<DurationDiscount> discounts;
    for (const Row& r : rowsOf(rows, "PRULE")) {
        r.expectFields(3);
        atLine(r.line, [&] {
            const std::string& key = r.s(1);
            if (key == "weekend") pr.setWeekendFactorBp(r.num(2));
            else if (key == "holiday") pr.setHolidayFactorBp(r.num(2));
            else if (key == "cap") pr.setMaxDailyFactorBp(r.num(2));
            else if (key == "quote_minutes") pr.setQuoteValidityMinutes(static_cast<int>(r.num(2)));
            else fail(r.line, "unknown pricing rule '" + key + "'");
        });
    }
    for (const Row& r : rowsOf(rows, "PTIER")) {
        r.expectFields(3);
        tiers.push_back({r.num(1), r.num(2)});
    }
    for (const Row& r : rowsOf(rows, "PDISC")) {
        r.expectFields(3);
        discounts.push_back({r.num(1), r.num(2)});
    }
    const std::size_t ruleLine = rowsOf(rows, "PTIER").empty() ? 1 : rowsOf(rows, "PTIER").front().line;
    atLine(ruleLine, [&] {
        if (!tiers.empty()) pr.setDemandTiers(tiers);
        pr.setDurationDiscounts(discounts);
    });
    for (const Row& r : rowsOf(rows, "HOLIDAY")) {
        r.expectFields(2);
        pr.addHoliday(r.date(1));
    }
    data.pricingRules = pr;

    ChargeRules cr = ChargeRules::defaults();
    for (const Row& r : rowsOf(rows, "CRULE")) {
        r.expectFields(3);
        if (r.s(1) != "late_multiplier") fail(r.line, "unknown charge rule '" + r.s(1) + "'");
        atLine(r.line, [&] { cr.setLateDayMultiplierBp(r.num(2)); });
    }
    for (const Row& r : rowsOf(rows, "CCAT")) {
        r.expectFields(5);
        atLine(r.line, [&] {
            cr.setForCategory(parseVehicleCategory(r.s(1)), {r.num(2), r.money(3), r.money(4)});
        });
    }
    data.chargeRules = cr;

    for (const Row& r : rowsOf(rows, "SETTING")) {
        r.expectFields(3);
        if (r.s(1) != "service_warning_km") fail(r.line, "unknown setting '" + r.s(1) + "'");
        if (r.num(2) < 0) fail(r.line, "service warning distance cannot be negative");
        data.serviceWarningKm = r.num(2);
    }
}

void loadFleetAndCustomers(const RowsByType& rows, RentalData& data) {
    for (const Row& r : rowsOf(rows, "MODEL")) {
        r.expectFields(6);
        atLine(r.line, [&] {
            data.models.add(VehicleModel(r.s(1), r.s(2), r.s(3), parseVehicleCategory(r.s(4)), r.money(5)));
        });
    }
    for (const Row& r : rowsOf(rows, "VEHICLE")) {
        r.expectFields(10);
        atLine(r.line, [&] {
            const VehicleCategory category = parseVehicleCategory(r.s(2));
            VehicleData d;
            d.id = r.s(1);
            d.registration = r.s(3);
            d.modelId = r.s(4);
            d.fuelType = parseFuelType(r.s(5));
            d.mileageKm = r.num(6);
            d.status = parseVehicleStatus(r.s(7));
            d.nextServiceDueKm = r.num(8);
            const VehicleModel* model = data.models.find(d.modelId);
            if (model == nullptr) fail(r.line, "vehicle " + d.id + " refers to missing model " + d.modelId);
            if (model->category() != category) {
                fail(r.line, "vehicle " + d.id + " is a " + toString(category) + " but model " + d.modelId + " is a " +
                                 toString(model->category()));
            }
            data.vehicles.add(VehicleFactory::instance().create(category, d, static_cast<int>(r.num(9))));
        });
    }
    std::set<std::string> licences;
    for (const Row& r : rowsOf(rows, "CUSTOMER")) {
        // 8 fields = older files (no customer type, treated as Regular); 9 = with type.
        if (r.f.size() != 8 && r.f.size() != 9) r.expectFields(9);
        atLine(r.line, [&] {
            std::set<VehicleCategory> cats;
            std::stringstream ss(r.s(7));
            std::string token;
            while (std::getline(ss, token, ',')) cats.insert(parseVehicleCategory(token));
            const CustomerType kind = r.f.size() == 9 ? parseCustomerType(r.s(8)) : CustomerType::Regular;
            const Customer& c = data.customers.add(CustomerFactory::create(
                kind, r.s(1), r.s(2), r.s(3), r.s(4), DrivingLicence(r.s(5), r.date(6), cats)));
            if (!licences.insert(c.licence().number()).second) {
                fail(r.line, "licence " + c.licence().number() + " is used by more than one customer");
            }
        });
    }
}

PriceBreakdown buildPrice(const Row& priceRow, const std::vector<const Row*>& dayRows) {
    PriceBreakdown p(DateRange(priceRow.date(3), priceRow.date(4)), priceRow.s(5), priceRow.money(6));
    const std::vector<Date> days = p.period().days();
    if (dayRows.size() != days.size()) {
        fail(priceRow.line, "price part has " + std::to_string(dayRows.size()) + " day lines but the period has " +
                                std::to_string(days.size()) + " days");
    }
    for (std::size_t i = 0; i < dayRows.size(); ++i) {
        const Row& r = *dayRows[i];
        DailyPrice d;
        d.date = r.date(3);
        if (d.date != days[i]) fail(r.line, "day line date " + d.date.toString() + " is out of order");
        d.booked = r.num(4);
        d.capacity = r.num(5);
        d.demandFactorBp = r.num(6);
        d.demandReason = r.s(7);
        d.calendarFactorBp = r.num(8);
        d.calendarReason = r.s(9);
        d.appliedFactorBp = r.num(10);
        d.capped = r.num(11) != 0;
        d.amount = r.money(12);
        p.addDay(d);
    }
    p.applyDurationDiscount(priceRow.num(7));
    if (p.subtotal() != priceRow.money(8) || p.total() != priceRow.money(9)) {
        fail(priceRow.line, "stored subtotal/total do not match the daily lines (file edited or corrupted)");
    }
    return p;
}

void loadBookings(const RowsByType& rows, RentalData& data) {
    // Group child records by booking ID.
    std::map<std::string, std::map<long long, const Row*>> prices;
    std::map<std::string, std::map<long long, std::vector<const Row*>>> days;
    std::map<std::string, std::map<std::string, const Row*>> inspections;
    for (const Row& r : rowsOf(rows, "PRICE")) {
        r.expectFields(10);
        if (!prices[r.s(1)].emplace(r.num(2), &r).second) fail(r.line, "duplicate price part");
    }
    for (const Row& r : rowsOf(rows, "PDAY")) {
        r.expectFields(13);
        days[r.s(1)][r.num(2)].push_back(&r);
    }
    for (const Row& r : rowsOf(rows, "INSPECT")) {
        r.expectFields(7);
        if (r.s(2) != "PICKUP" && r.s(2) != "RETURN") fail(r.line, "inspection kind must be PICKUP or RETURN");
        if (!inspections[r.s(1)].emplace(r.s(2), &r).second) fail(r.line, "duplicate " + r.s(2) + " inspection");
    }

    std::set<std::string> bookingIds;
    for (const Row& r : rowsOf(rows, "BOOKING")) {
        r.expectFields(8);
        const std::string id = r.s(1);
        bookingIds.insert(id);
        atLine(r.line, [&] {
            if (data.customers.find(r.s(2)) == nullptr) fail(r.line, "booking " + id + " refers to missing customer " + r.s(2));
            if (data.vehicles.find(r.s(3)) == nullptr) fail(r.line, "booking " + id + " refers to missing vehicle " + r.s(3));
            const BookingStatus status = parseBookingStatus(r.s(4));
            const auto& parts = prices[id];
            if (parts.empty()) fail(r.line, "booking " + id + " has no price");
            long long expectedSeq = 0;
            for (const auto& part : parts) {
                if (part.first != expectedSeq++) fail(part.second->line, "price parts must be numbered 0, 1, 2...");
            }
            std::vector<PriceBreakdown> built;
            for (const auto& part : parts) {
                built.push_back(atLine(part.second->line, [&] { return buildPrice(*part.second, days[id][part.first]); }));
            }

            // Replay the booking's history through its own validated methods.
            Booking b(id, r.s(2), r.s(3), built.front(), r.num(5));
            for (std::size_t i = 1; i < built.size(); ++i) b.addExtension(built[i]);
            const auto& insp = inspections[id];
            const auto toReport = [](const Row& x) { return InspectionReport(x.date(3), x.num(4), static_cast<int>(x.num(5)), x.s(6)); };
            if (insp.count("PICKUP")) b.recordPickup(toReport(*insp.at("PICKUP")), r.money(6));
            if (status == BookingStatus::Cancelled) b.cancel();
            if (insp.count("RETURN")) b.recordReturn(toReport(*insp.at("RETURN")), r.s(7));
            if (b.status() != status) {
                fail(r.line, "booking " + id + " is stored as " + toString(status) +
                                 " but its pickup/return records imply " + toString(b.status()));
            }
            data.bookings.add(std::move(b));
        });
    }
    // Orphaned child records point at bookings that do not exist.
    for (const auto& type : {"PRICE", "PDAY", "INSPECT"}) {
        for (const Row& r : rowsOf(rows, type)) {
            if (bookingIds.count(r.s(1)) == 0) fail(r.line, std::string(type) + " refers to missing booking " + r.s(1));
        }
    }
    for (const auto& entry : days) {
        for (const auto& seq : entry.second) {
            if (prices[entry.first].count(seq.first) == 0) {
                fail(seq.second.front()->line, "day lines refer to missing price part " + std::to_string(seq.first));
            }
        }
    }
    // No vehicle may be double-booked.
    const auto all = data.bookings.all();
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            const Booking& a = *all[i];
            const Booking& b = *all[j];
            if (a.vehicleId() == b.vehicleId() && a.occupiesVehicle() && b.occupiesVehicle() &&
                (a.period().overlaps(b.period()) ||
                 (a.status() == BookingStatus::Active && b.status() == BookingStatus::Active))) {
                throw PersistenceError("Data file: bookings " + a.id() + " and " + b.id() + " clash on vehicle " +
                                       a.vehicleId() + ".");
            }
        }
    }
}

void loadInvoicesAndMaintenance(const RowsByType& rows, RentalData& data) {
    std::map<std::string, std::vector<const Row*>> lines;
    for (const Row& r : rowsOf(rows, "INVLINE")) {
        r.expectFields(5);
        lines[r.s(1)].push_back(&r);
    }
    std::set<std::string> invoiceIds;
    for (const Row& r : rowsOf(rows, "INVOICE")) {
        r.expectFields(8);
        invoiceIds.insert(r.s(1));
        atLine(r.line, [&] {
            const Booking* b = data.bookings.find(r.s(2));
            if (b == nullptr) fail(r.line, "invoice " + r.s(1) + " refers to missing booking " + r.s(2));
            if (b->invoiceId() != r.s(1)) fail(r.line, "booking " + b->id() + " does not reference invoice " + r.s(1));
            Invoice inv(r.s(1), r.s(2), r.s(3), r.s(4), r.date(5), r.money(6));
            for (const Row* l : lines[r.s(1)]) {
                atLine(l->line, [&] { inv.addLine({parseChargeType(l->s(2)), l->s(3), l->money(4)}); });
            }
            if (parseInvoiceStatus(r.s(7)) == InvoiceStatus::Finalized) inv.finalize();
            data.invoices.add(std::move(inv));
        });
    }
    for (const Row& r : rowsOf(rows, "INVLINE")) {
        if (invoiceIds.count(r.s(1)) == 0) fail(r.line, "invoice line refers to missing invoice " + r.s(1));
    }
    for (const Booking* b : data.bookings.all()) {
        if (b->status() == BookingStatus::Completed && data.invoices.find(b->invoiceId()) == nullptr) {
            throw PersistenceError("Data file: completed booking " + b->id() + " has no invoice " + b->invoiceId() + ".");
        }
    }

    std::map<std::string, int> openPerVehicle;
    for (const Row& r : rowsOf(rows, "MAINT")) {
        r.expectFields(10);
        atLine(r.line, [&] {
            if (data.vehicles.find(r.s(2)) == nullptr) fail(r.line, "maintenance refers to missing vehicle " + r.s(2));
            MaintenanceRecord m(r.s(1), r.s(2), r.s(4), r.date(5), r.num(6));
            if (parseMaintenanceStatus(r.s(3)) == MaintenanceStatus::Completed) {
                m.complete(r.date(7), r.num(8), r.money(9));
            } else {
                ++openPerVehicle[m.vehicleId()];
            }
            data.maintenance.add(std::move(m));
        });
    }
    for (const Vehicle* v : data.vehicles.all()) {
        const int open = openPerVehicle[v->id()];
        if (open > 1) throw PersistenceError("Data file: vehicle " + v->id() + " has more than one open maintenance record.");
        if (v->status() == VehicleStatus::UnderMaintenance && open == 0) {
            throw PersistenceError("Data file: vehicle " + v->id() + " is UnderMaintenance without an open maintenance record.");
        }
        if (v->status() == VehicleStatus::Active && open == 1) {
            throw PersistenceError("Data file: vehicle " + v->id() + " is Active but has an open maintenance record.");
        }
    }
}

} // namespace

RentalData DataStore::deserialize(const std::string& content) {
    const RowsByType rows = parseLines(content);
    RentalData data;
    loadRules(rows, data);
    loadFleetAndCustomers(rows, data);
    loadBookings(rows, data);
    loadInvoicesAndMaintenance(rows, data);

    for (const Row& r : rowsOf(rows, "COUNTER")) {
        r.expectFields(3);
        data.ids.setCounter(r.s(1), r.num(2));
    }
    // Never hand out an ID that already exists, even if a counter was wrong.
    for (const VehicleModel* m : data.models.all()) data.ids.observe(m->id());
    for (const Vehicle* v : data.vehicles.all()) data.ids.observe(v->id());
    for (const Customer* c : data.customers.all()) data.ids.observe(c->id());
    for (const Booking* b : data.bookings.all()) data.ids.observe(b->id());
    for (const Invoice* i : data.invoices.all()) data.ids.observe(i->id());
    for (const MaintenanceRecord* m : data.maintenance.all()) data.ids.observe(m->id());
    return data;
}

RentalData DataStore::load() const {
    std::ifstream in(file_, std::ios::binary);
    if (!in) throw PersistenceError("Cannot open data file " + file_.string() + ".");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    try {
        return deserialize(buffer.str());
    } catch (const PersistenceError& e) {
        throw PersistenceError(file_.string() + ": " + e.what());
    }
}

void DataStore::save(const RentalData& data) const {
    const std::string content = serialize(data);
    deserialize(content); // refuse to write anything we could not read back

    std::error_code ec;
    if (file_.has_parent_path()) {
        fs::create_directories(file_.parent_path(), ec);
        if (ec) throw PersistenceError("Cannot create folder " + file_.parent_path().string() + ": " + ec.message());
    }
    {
        std::ofstream out(tempPath(), std::ios::binary | std::ios::trunc);
        if (!out) throw PersistenceError("Cannot write " + tempPath().string() + ".");
        out << content;
        out.flush();
        if (!out) throw PersistenceError("Writing " + tempPath().string() + " failed (disk full?).");
    } // file closed here (RAII)

    if (fs::exists(file_)) {
        fs::remove(backupPath(), ec); // ignore "not found"
        fs::rename(file_, backupPath(), ec);
        if (ec) throw PersistenceError("Cannot create backup " + backupPath().string() + ": " + ec.message());
    }
    fs::rename(tempPath(), file_, ec);
    if (ec) {
        std::error_code restore;
        fs::rename(backupPath(), file_, restore); // put the previous version back
        throw PersistenceError("Cannot replace " + file_.string() + ": " + ec.message());
    }
}

} // namespace rental
