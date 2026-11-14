#include "rental/DataStore.h"
#include "rental/DemoData.h"
#include "rental/RentalSystem.h"
#include "rental/Clock.h"
#include "rental/AuditLogger.h"
#include "rental/NotificationService.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <thread>
#include <map>
#include <set>
#include <vector>

using namespace rental;
namespace beast=boost::beast; namespace http=beast::http; namespace net=boost::asio;
using tcp=net::ip::tcp; using pt=boost::property_tree::ptree;
static SystemClock clockSource;
static std::unique_ptr<RentalSystem> app;
static std::unique_ptr<DataStore> store;
static std::shared_ptr<AuditLogger> audit; // Observer: feeds the dashboard's "Recent activity"
static std::string get(const pt& p,const std::string& k,const std::string& d=""){return p.get<std::string>(k,d);}
static long long num(const pt& p,const std::string& k,long long d=0){return p.get<long long>(k,d);}
static std::set<VehicleCategory> parseCategories(const std::string& text){std::set<VehicleCategory> result;std::istringstream input(text);std::string item;while(std::getline(input,item,',')){const auto first=item.find_first_not_of(" ");if(first!=std::string::npos){const auto last=item.find_last_not_of(" ");result.insert(parseVehicleCategory(item.substr(first,last-first+1)));}}if(result.empty())throw ValidationError("Select at least one permitted vehicle category.");return result;}static pt arr(){return pt{};} static void push(pt& a,pt v){a.push_back({"",v});}
static pt state(){
  pt root,vehicles,customers,bookings,maintenance,activity,monthly;
  std::map<std::string,int> customerBookingCounts;
  std::map<std::string,std::string> customerRentalStatus;
  std::set<std::string> occupied;
  for(const Booking* b:app->rentals().bookings()){
    if(b->occupiesVehicle())occupied.insert(b->vehicleId());
    ++customerBookingCounts[b->customerId()];
    if(b->status()==BookingStatus::Active)customerRentalStatus[b->customerId()]="On rent";
    else if(b->status()==BookingStatus::Confirmed && customerRentalStatus[b->customerId()]!="On rent")customerRentalStatus[b->customerId()]="Upcoming";
  }
  for(const Vehicle* v:app->fleet().vehicles()){
    const auto& m=app->fleet().modelOf(*v);pt x;
    x.put("id",v->id());x.put("name",m.displayName());x.put("type",toString(v->category())+" · "+v->specSummary());
    x.put("plate",v->registration());x.put("rate",m.baseDailyRate().paise()/100.0);x.put("mileage",v->mileageKm());
    x.put("status",v->status()==VehicleStatus::UnderMaintenance?"Maintenance":(v->status()==VehicleStatus::Inactive?"Inactive":(occupied.count(v->id())?"Booked":"Available")));
    x.put("icon",v->category()==VehicleCategory::Bike?"🏍️":(v->category()==VehicleCategory::Van?"🚐":"🚙"));push(vehicles,x);
  }
  for(const Customer* c:app->customers().customers()){
    pt x;x.put("id",c->id());x.put("name",c->name());x.put("email",c->email());x.put("phone",c->phone());
    x.put("licence",c->licence().number());x.put("expiry",c->licence().expiry().toString());x.put("categories",c->licence().categoriesText());
    x.put("bookings",customerBookingCounts[c->id()]);x.put("rentalStatus",customerRentalStatus.count(c->id())?customerRentalStatus[c->id()]:"Registered");
    x.put("type",c->typeName());x.put("discount",c->discountBp()/100.0);x.put("limit",c->maxActiveBookings());push(customers,x);
  }
  for(const Booking* b:app->rentals().bookings()){
    const auto& c=app->customers().customer(b->customerId());const auto& v=app->fleet().vehicle(b->vehicleId());pt x;
    x.put("id",b->id());x.put("customerId",b->customerId());x.put("customer",c.name());x.put("vehicleId",b->vehicleId());
    x.put("vehicle",app->fleet().modelOf(v).displayName());x.put("dates",b->period().start().toString()+" – "+b->period().end().toString());
    x.put("amount",b->agreedRentalTotal().paise()/100.0);x.put("status",toString(b->status()));
    if(b->pickupReport()){x.put("pickupMileage",b->pickupReport()->odometerKm());x.put("pickupFuel",b->pickupReport()->fuelPercent());}
    push(bookings,x);
  }
  for(const auto& m:app->data().maintenance.all()){pt x;x.put("vehicle",app->fleet().modelOf(app->fleet().vehicle(m->vehicleId())).displayName());x.put("issue",m->description());x.put("date",m->openedOn().toString());x.put("cost",m->cost().paise()/100.0);x.put("status",toString(m->status()));push(maintenance,x);}
  if(audit){for(const auto& e:audit->recent(8)){pt row;for(const std::string* part:{&e.symbol,&e.title,&e.text,&e.when}){pt cell;cell.put_value(*part);row.push_back({"",cell});}push(activity,row);}}
  root.put("revenue",app->reports().revenue(std::nullopt).total.paise()/100.0);
  const Date today=clockSource.today();const int currentSerial=today.year()*12+today.month()-1;std::vector<double> monthlyAmounts(12,0.0);
  for(const Invoice* invoice:app->data().invoices.all())if(invoice->status()==InvoiceStatus::Finalized){const Date d=invoice->issueDate();const int diff=currentSerial-(d.year()*12+d.month()-1);if(diff>=0&&diff<12)monthlyAmounts[static_cast<std::size_t>(11-diff)]+=invoice->totalCharges().paise()/100.0;}
  static const char* monthNames[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  for(int i=0;i<12;++i){const int serial=currentSerial-11+i;int year=serial/12;int month=serial%12+1;if(month<=0){month+=12;--year;}pt row;row.put("label",monthNames[month-1]);row.put("year",year);row.put("amount",monthlyAmounts[static_cast<std::size_t>(i)]);push(monthly,row);}
  root.put("chartPeriod",std::string("Last 12 months through ")+monthNames[today.month()-1]+" "+std::to_string(today.year()));
  root.add_child("monthlyRevenue",monthly);root.add_child("vehicles",vehicles);root.add_child("customers",customers);root.add_child("bookings",bookings);root.add_child("maintenance",maintenance);root.add_child("activity",activity);return root;
}
static pt quoteData(const PriceQuote& quote){
  const PriceBreakdown& b=quote.breakdown;pt out,days;out.put("id",quote.id);out.put("strategy",b.strategyName());
  out.put("baseDailyRate",b.baseDailyRate().paise()/100.0);out.put("subtotal",b.subtotal().paise()/100.0);
  out.put("discount",b.discount().paise()/100.0);out.put("discountPercent",b.discountBp()/100.0);out.put("total",b.total().paise()/100.0);
  out.put("expiresAt",quote.expiresAt);
  for(const DailyPrice& day:b.days()){pt row;row.put("date",day.date.toString());row.put("amount",day.amount.paise()/100.0);row.put("demand",day.demandReason);row.put("calendar",day.calendarReason);row.put("factor",formatFactor(day.appliedFactorBp));push(days,row);}
  out.add_child("days",days);return out;
}static std::string json(const pt& p){std::ostringstream o;boost::property_tree::write_json(o,p,false);return o.str();}
static http::response<http::string_body> response(http::status s,const std::string& body,unsigned v=11){http::response<http::string_body> r{s,v};r.set(http::field::content_type,"application/json; charset=utf-8");r.set(http::field::access_control_allow_origin,"*");r.set(http::field::access_control_allow_headers,"Content-Type");r.set(http::field::access_control_allow_methods,"GET, POST, PUT, DELETE, OPTIONS");r.body()=body;r.prepare_payload();return r;}
static http::response<http::string_body> handle(const http::request<http::string_body>& req){
 try{
  if(req.method()==http::verb::options)return response(http::status::no_content,"");
  const std::string target(req.target());
  if(req.method()==http::verb::get && target=="/api/state")return response(http::status::ok,json(state()));
  pt p; if(!req.body().empty()){std::istringstream in(req.body());boost::property_tree::read_json(in,p);}
  if(req.method()==http::verb::put && target.rfind("/api/customers/",0)==0){
   const std::string prefix="/api/customers/";const std::string id=target.substr(prefix.size());
   if(id.empty()||id.find('/')!=std::string::npos)return response(http::status::not_found,"{\"error\":\"Route not found\"}");
   const Customer& current=app->customers().customer(id);
   DrivingLicence licence(get(p,"licence",current.licence().number()),Date::parse(get(p,"expiry",current.licence().expiry().toString())),parseCategories(get(p,"categories",current.licence().categoriesText())));
   app->customers().updateDetails(id,get(p,"name",current.name()),get(p,"phone",current.phone()),get(p,"email",current.email()),licence);
   store->save(app->data());return response(http::status::ok,"{\"ok\":true}");
  }  if(req.method()==http::verb::delete_ && target.rfind("/api/customers/",0)==0){
   const std::string prefix="/api/customers/";const std::string id=target.substr(prefix.size());
   if(id.empty()||id.find('/')!=std::string::npos)return response(http::status::not_found,"{\"error\":\"Route not found\"}");
   app->customers().removeCustomer(id);store->save(app->data());return response(http::status::ok,"{\"ok\":true}");
  }  if(req.method()==http::verb::post && target=="/api/customers"){
   auto cats=parseCategories(get(p,"categories","Car,Bike,Van"));
   DrivingLicence lic(get(p,"licence"),Date::parse(get(p,"expiry","2035-12-31")),cats);
   auto& c=app->customers().registerCustomer(parseCustomerType(get(p,"type","Regular")),get(p,"name"),get(p,"phone"),get(p,"email"),lic);store->save(app->data());pt out;out.put("id",c.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/vehicles"){
   auto cat=parseVehicleCategory(get(p,"category","Car"));auto model=app->fleet().addModel(get(p,"brand","Generic"),get(p,"model",get(p,"name","Vehicle")),cat,Money::parse(get(p,"rate","1000")));
   int spec=static_cast<int>(num(p,"spec",cat==VehicleCategory::Bike?150:5));auto& v=app->fleet().addVehicle(model.id(),get(p,"plate"),parseFuelType(get(p,"fuel","Petrol")),num(p,"mileage",0),spec);store->save(app->data());pt out;out.put("id",v.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/maintenance"){
   auto& m=app->maintenance().openMaintenance(get(p,"vehicleId"),get(p,"description"),Date::parse(get(p,"date",clockSource.today().toString())));store->save(app->data());pt out;out.put("id",m.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/quotes"){
   auto& q=app->quotes().createQuote(get(p,"customerId"),get(p,"vehicleId"),DateRange(Date::parse(get(p,"start")),Date::parse(get(p,"end"))));return response(http::status::ok,json(quoteData(q)));
  }  if(req.method()==http::verb::post && target=="/api/bookings"){
   const std::string quoteId=get(p,"quoteId");std::string actualQuoteId=quoteId;
   if(actualQuoteId.empty())actualQuoteId=app->quotes().createQuote(get(p,"customerId"),get(p,"vehicleId"),DateRange(Date::parse(get(p,"start")),Date::parse(get(p,"end")))).id;
   auto& b=app->rentals().confirmQuote(actualQuoteId);store->save(app->data());pt out;out.put("id",b.id());out.put("amount",b.agreedRentalTotal().paise()/100.0);return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target.rfind("/api/bookings/",0)==0){const std::string prefix="/api/bookings/";auto rest=target.substr(prefix.size());auto slash=rest.find('/');auto id=rest.substr(0,slash);auto action=slash==std::string::npos?"":rest.substr(slash+1);
   if(action=="cancel"){app->rentals().cancelBooking(id);store->save(app->data());return response(http::status::ok,"{\"ok\":true}");}
   if(action=="pickup"){auto& b=app->rentals().booking(id);auto& v=app->fleet().vehicle(b.vehicleId());app->rentals().pickup(id,InspectionReport(clockSource.today(),num(p,"odometer",v.mileageKm()),static_cast<int>(num(p,"fuel",100)),get(p,"notes","Web pickup inspection")));store->save(app->data());return response(http::status::ok,"{\"ok\":true}");}
   if(action=="return"){auto& b=app->rentals().booking(id);std::vector<DamageCharge> damages;const std::string reason=get(p,"damageReason");const std::string amount=get(p,"damageAmount","0");const Money damage=Money::parse(amount);if(damage.paise()>0)damages.emplace_back(reason,damage);auto& v=app->fleet().vehicle(b.vehicleId());auto& invoice=app->rentals().completeReturn(id,InspectionReport(clockSource.today(),num(p,"odometer",v.mileageKm()),static_cast<int>(num(p,"fuel",100)),get(p,"notes","Web return inspection")),damages);store->save(app->data());pt out;out.put("ok",true);out.put("invoiceTotal",invoice.totalCharges().paise()/100.0);out.put("depositRefunded",invoice.depositRefunded().paise()/100.0);return response(http::status::ok,json(out));}
  }
  return response(http::status::not_found,"{\"error\":\"Route not found\"}");
 }catch(const std::exception& e){pt err;err.put("error",e.what());return response(http::status::bad_request,json(err));}
}
static void session(tcp::socket socket){try{beast::flat_buffer buffer;http::request<http::string_body> req;http::read(socket,buffer,req);auto res=handle(req);http::write(socket,res);beast::error_code ec;socket.shutdown(tcp::socket::shutdown_send,ec);}catch(...) {}}
int main(int argc,char**argv){try{std::string file=argc>1?argv[1]:"data/rental_data.txt";store=std::make_unique<DataStore>(file);RentalData data;if(store->exists())data=store->load();else{ManualClock seedClock(clockSource.today());RentalSystem seed(RentalData{},seedClock);populateDemoData(seed,seedClock,clockSource.today());store->save(seed.data());data=store->load();}app=std::make_unique<RentalSystem>(std::move(data),clockSource);audit=std::make_shared<AuditLogger>(app->data(),std::filesystem::path(file).parent_path()/"audit_log.txt");app->rentals().addObserver(audit);app->rentals().addObserver(std::make_shared<NotificationService>(app->data(),std::cout));net::io_context ioc{1};tcp::acceptor acceptor{ioc,{net::ip::make_address("127.0.0.1"),8765}};std::cout<<"Smart Vehicle Rental API running at http://127.0.0.1:8765\n";for(;;){tcp::socket socket{ioc};acceptor.accept(socket);std::thread(session,std::move(socket)).detach();}}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}









