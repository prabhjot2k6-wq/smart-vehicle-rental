#include "rental/DataStore.h"
#include "rental/DemoData.h"
#include "rental/RentalSystem.h"
#include "rental/Clock.h"
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

using namespace rental;
namespace beast=boost::beast; namespace http=beast::http; namespace net=boost::asio;
using tcp=net::ip::tcp; using pt=boost::property_tree::ptree;
static SystemClock clockSource;
static std::unique_ptr<RentalSystem> app;
static std::unique_ptr<DataStore> store;
static std::string get(const pt& p,const std::string& k,const std::string& d=""){return p.get<std::string>(k,d);}
static long long num(const pt& p,const std::string& k,long long d=0){return p.get<long long>(k,d);}
static pt arr(){return pt{};} static void push(pt& a,pt v){a.push_back({"",v});}
static pt state(){
  pt root,vehicles,customers,bookings,maintenance,activity;
  std::map<std::string,int> customerBookingCounts; std::set<std::string> occupied;
  for(auto b:app->rentals().bookings()){if(b->occupiesVehicle())occupied.insert(b->vehicleId());++customerBookingCounts[b->customerId()];}
  for(auto v:app->fleet().vehicles()) { const auto& m=app->fleet().modelOf(*v); pt x; x.put("id",v->id()); x.put("name",m.displayName()); x.put("type",toString(v->category())+" · "+v->specSummary()); x.put("plate",v->registration()); x.put("rate",m.baseDailyRate().paise()/100.0); x.put("status",v->status()==VehicleStatus::UnderMaintenance?"Maintenance":(v->status()==VehicleStatus::Inactive?"Inactive":(occupied.count(v->id())?"Booked":"Available"))); x.put("icon",v->category()==VehicleCategory::Bike?"🏍️":(v->category()==VehicleCategory::Van?"🚐":"🚙")); push(vehicles,x); }
  for(auto c:app->customers().customers()){pt x;x.put("id",c->id());x.put("name",c->name());x.put("email",c->email());x.put("phone",c->phone());x.put("licence",c->licence().number());x.put("bookings",customerBookingCounts[c->id()]);push(customers,x);}
  for(auto b:app->rentals().bookings()){pt x;const auto& c=app->customers().customer(b->customerId());const auto& v=app->fleet().vehicle(b->vehicleId());x.put("id",b->id());x.put("customer",c.name());x.put("vehicle",app->fleet().modelOf(v).displayName());x.put("dates",b->period().start().toString()+" – "+b->period().end().toString());x.put("amount",b->agreedRentalTotal().paise()/100.0);x.put("status",toString(b->status()));push(bookings,x);}
  for(auto m:app->data().maintenance.all()){pt x;x.put("vehicle",app->fleet().modelOf(app->fleet().vehicle(m->vehicleId())).displayName());x.put("issue",m->description());x.put("date",m->openedOn().toString());x.put("cost",m->cost().paise()/100.0);x.put("status",toString(m->status()));push(maintenance,x);}
  root.put("revenue",app->reports().revenue(std::nullopt).total.paise()/100.0);
  root.add_child("vehicles",vehicles);root.add_child("customers",customers);root.add_child("bookings",bookings);root.add_child("maintenance",maintenance);root.add_child("activity",activity);return root;
}
static std::string json(const pt& p){std::ostringstream o;boost::property_tree::write_json(o,p,false);return o.str();}
static http::response<http::string_body> response(http::status s,const std::string& body,unsigned v=11){http::response<http::string_body> r{s,v};r.set(http::field::content_type,"application/json; charset=utf-8");r.set(http::field::access_control_allow_origin,"*");r.set(http::field::access_control_allow_headers,"Content-Type");r.set(http::field::access_control_allow_methods,"GET, POST, OPTIONS");r.body()=body;r.prepare_payload();return r;}
static http::response<http::string_body> handle(const http::request<http::string_body>& req){
 try{
  if(req.method()==http::verb::options)return response(http::status::no_content,"");
  const std::string target(req.target());
  if(req.method()==http::verb::get && target=="/api/state")return response(http::status::ok,json(state()));
  pt p; if(!req.body().empty()){std::istringstream in(req.body());boost::property_tree::read_json(in,p);}
  if(req.method()==http::verb::post && target=="/api/customers"){
   auto cats=std::set<VehicleCategory>{VehicleCategory::Car,VehicleCategory::Bike,VehicleCategory::Van};
   DrivingLicence lic(get(p,"licence"),Date::parse(get(p,"expiry","2035-12-31")),cats);
   auto& c=app->customers().registerCustomer(get(p,"name"),get(p,"phone"),get(p,"email"),lic);store->save(app->data());pt out;out.put("id",c.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/vehicles"){
   auto cat=parseVehicleCategory(get(p,"category","Car"));auto model=app->fleet().addModel(get(p,"brand","Generic"),get(p,"model",get(p,"name","Vehicle")),cat,Money::parse(get(p,"rate","1000")));
   int spec=static_cast<int>(num(p,"spec",cat==VehicleCategory::Bike?150:5));auto& v=app->fleet().addVehicle(model.id(),get(p,"plate"),parseFuelType(get(p,"fuel","Petrol")),num(p,"mileage",0),spec);store->save(app->data());pt out;out.put("id",v.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/maintenance"){
   auto& m=app->maintenance().openMaintenance(get(p,"vehicleId"),get(p,"description"),Date::parse(get(p,"date",clockSource.today().toString())));store->save(app->data());pt out;out.put("id",m.id());return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target=="/api/bookings"){
   auto period=DateRange(Date::parse(get(p,"start")),Date::parse(get(p,"end")));
   auto& q=app->quotes().createQuote(get(p,"customerId"),get(p,"vehicleId"),period);auto& b=app->rentals().confirmQuote(q.id);store->save(app->data());pt out;out.put("id",b.id());out.put("amount",b.agreedRentalTotal().paise()/100.0);return response(http::status::created,json(out));
  }
  if(req.method()==http::verb::post && target.rfind("/api/bookings/",0)==0){auto rest=target.substr(13);auto slash=rest.find('/');auto id=rest.substr(0,slash);auto action=slash==std::string::npos?"":rest.substr(slash+1);
   if(action=="cancel"){app->rentals().cancelBooking(id);store->save(app->data());return response(http::status::ok,"{\"ok\":true}");}
   if(action=="pickup"){auto& b=app->rentals().booking(id);app->rentals().pickup(id,InspectionReport(clockSource.today(),app->fleet().vehicle(b.vehicleId()).mileageKm(),100,"Web pickup inspection"));store->save(app->data());return response(http::status::ok,"{\"ok\":true}");}
   if(action=="return"){auto& b=app->rentals().booking(id);app->rentals().completeReturn(id,InspectionReport(clockSource.today(),app->fleet().vehicle(b.vehicleId()).mileageKm(),100,"Web return inspection"),{});store->save(app->data());return response(http::status::ok,"{\"ok\":true}");}
  }
  return response(http::status::not_found,"{\"error\":\"Route not found\"}");
 }catch(const std::exception& e){pt err;err.put("error",e.what());return response(http::status::bad_request,json(err));}
}
static void session(tcp::socket socket){try{beast::flat_buffer buffer;http::request<http::string_body> req;http::read(socket,buffer,req);auto res=handle(req);http::write(socket,res);beast::error_code ec;socket.shutdown(tcp::socket::shutdown_send,ec);}catch(...) {}}
int main(int argc,char**argv){try{std::string file=argc>1?argv[1]:"data/rental_data.txt";store=std::make_unique<DataStore>(file);RentalData data;if(store->exists())data=store->load();else{ManualClock seedClock(clockSource.today());RentalSystem seed(RentalData{},seedClock);populateDemoData(seed,seedClock,clockSource.today());store->save(seed.data());data=store->load();}app=std::make_unique<RentalSystem>(std::move(data),clockSource);net::io_context ioc{1};tcp::acceptor acceptor{ioc,{net::ip::make_address("127.0.0.1"),8765}};std::cout<<"Smart Vehicle Rental API running at http://127.0.0.1:8765\n";for(;;){tcp::socket socket{ioc};acceptor.accept(socket);std::thread(session,std::move(socket)).detach();}}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
