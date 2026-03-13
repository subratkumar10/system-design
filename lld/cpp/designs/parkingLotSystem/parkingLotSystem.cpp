#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
using namespace std;
using Clock = chrono::steady_clock;
using TimePoint = chrono::time_point<Clock>;

// --------------- Utilities ---------------
struct Money {
    int cents{0};
    static Money fromDollars(double d) { return Money{static_cast<int>(llround(d * 100))}; }
    double dollars() const { return cents / 100.0; }
    Money operator+(const Money& other) const { return Money{cents + other.cents}; }
};

enum class VehicleType { Motorcycle, Car, Truck, EV };
enum class SpotType { Motorcycle, Compact, Large, EV };

// --------------- Vehicles ---------------
class Vehicle {
public:
    virtual ~Vehicle() = default;
    virtual VehicleType type() const = 0;
    virtual string plate() const = 0;
};

class BaseVehicle : public Vehicle {
    string plate_;
public:
    explicit BaseVehicle(string plate) : plate_(std::move(plate)) {}
    string plate() const override { return plate_; }
};

class MotorcycleV : public BaseVehicle {
public:
    using BaseVehicle::BaseVehicle;
    VehicleType type() const override { return VehicleType::Motorcycle; }
};
class CarV : public BaseVehicle {
public:
    using BaseVehicle::BaseVehicle;
    VehicleType type() const override { return VehicleType::Car; }
};
class TruckV : public BaseVehicle {
public:
    using BaseVehicle::BaseVehicle;
    VehicleType type() const override { return VehicleType::Truck; }
};
class EVV : public BaseVehicle {
public:
    using BaseVehicle::BaseVehicle;
    VehicleType type() const override { return VehicleType::EV; }
};

// --------------- Spots ---------------
class Spot {
    string id_;
    SpotType type_;
    bool occupied_{false};
public:
    Spot(string id, SpotType t) : id_(std::move(id)), type_(t) {}
    const string& id() const { return id_; }
    SpotType spotType() const { return type_; }
    bool isOccupied() const { return occupied_; }
    void occupy() { occupied_ = true; }
    void release() { occupied_ = false; }
};

class Floor {
    string name_;
    vector<Spot*> spots_; // raw pointers; Floor owns them
public:
    explicit Floor(string name) : name_(std::move(name)) {}
    ~Floor() {
        for (auto* s : spots_) delete s;
    }
    const string& name() const { return name_; }
    void addSpot(Spot* s) { spots_.push_back(s); }
    vector<Spot*> available(SpotType st) {
        vector<Spot*> res;
        for (auto* sp : spots_) if (!sp->isOccupied() && sp->spotType()==st) res.push_back(sp);
        return res;
    }
    vector<Spot*> allAvailable() {
        vector<Spot*> res;
        for (auto* sp : spots_) if (!sp->isOccupied()) res.push_back(sp);
        return res;
    }
    size_t capacity() const { return spots_.size(); }
    size_t used() const {
        size_t u=0; for (auto* sp:spots_) if (sp->isOccupied()) ++u; return u;
    }
};


// --------------- Observer for lot status ---------------
struct LotEvent {
    string message;
    double utilization; // 0..1
};

class LotObserver {
public:
    virtual ~LotObserver() = default;
    virtual void onEvent(const LotEvent& e) = 0;
};

// --------------- Allocation Strategy ---------------
class AllocationStrategy {
public:
    virtual ~AllocationStrategy() = default;
    virtual Spot* allocate(const vector<Floor*>& floors, const Vehicle& v) = 0;
};

static bool canFit(VehicleType vt, SpotType st) {
    switch (vt) {
        case VehicleType::Motorcycle: return true;
        case VehicleType::Car: return st==SpotType::Compact || st==SpotType::Large || st==SpotType::EV;
        case VehicleType::Truck: return st==SpotType::Large;
        case VehicleType::EV: return st==SpotType::EV || st==SpotType::Large;
    }
    return false;
}

class FirstFitAllocation : public AllocationStrategy {
public:
    Spot* allocate(const vector<Floor*>& floors, const Vehicle& v) override {
        vector<SpotType> pref;
        switch (v.type()) {
            case VehicleType::Motorcycle: pref={SpotType::Motorcycle, SpotType::Compact, SpotType::Large, SpotType::EV}; break;
            case VehicleType::Car: pref={SpotType::Compact, SpotType::Large, SpotType::EV}; break;
            case VehicleType::Truck: pref={SpotType::Large}; break;
            case VehicleType::EV: pref={SpotType::EV, SpotType::Large}; break;
        }
        for (auto* f : floors) {
            for (auto st : pref) {
                auto avail = f->available(st);
                if (!avail.empty()) return avail.front();
            }
        }
        for (auto* f : floors) {
            for (auto* s : f->allAvailable()) if (canFit(v.type(), s->spotType())) return s;
        }
        return nullptr;
    }
};

// --------------- Pricing Strategy ---------------
class PricingStrategy {
public:
    virtual ~PricingStrategy() = default;
    virtual Money computeFee(TimePoint entry, TimePoint exit, const Vehicle& v, SpotType st) const = 0;
};

class FlatRatePricing : public PricingStrategy {
    Money flat_;
public:
    explicit FlatRatePricing(Money m) : flat_(m) {}
    Money computeFee(TimePoint, TimePoint, const Vehicle&, SpotType) const override { return flat_; }
};

class HourlyPricing : public PricingStrategy {
    Money firstHour_;
    Money perHour_;
public:
    HourlyPricing(Money firstHour, Money perHour) : firstHour_(firstHour), perHour_(perHour) {}
    Money computeFee(TimePoint entry, TimePoint exit, const Vehicle&, SpotType) const override {
        using namespace chrono;
        auto mins = duration_cast<minutes>(exit - entry).count();
        if (mins <= 0) return Money{0};
        int hours = static_cast<int>(ceil(mins / 60.0));
        if (hours <= 1) return firstHour_;
        return firstHour_ + Money{perHour_.cents * (hours - 1)};
    }
};

class ProgressivePricing : public PricingStrategy {
public:
    Money computeFee(TimePoint entry, TimePoint exit, const Vehicle& v, SpotType st) const override {
        HourlyPricing base(Money::fromDollars(5.0), Money::fromDollars(3.0));
        Money m = base.computeFee(entry, exit, v, st);
        if (v.type() == VehicleType::Motorcycle) m.cents = static_cast<int>(m.cents * 0.6);
        if (st == SpotType::EV) m.cents = static_cast<int>(m.cents * 1.2);
        return m;
    }
};

// --------------- Ticket, Receipt, Factories ---------------
class Ticket {
    string id_;
    string plate_;
    Spot* spot_;
    TimePoint entry_;
public:
    Ticket(string id, string plate, Spot* spot, TimePoint entry)
        : id_(std::move(id)), plate_(std::move(plate)), spot_(spot), entry_(entry) {}
    const string& id() const { return id_; }
    const string& plate() const { return plate_; }
    Spot* spot() const { return spot_; }
    TimePoint entryTime() const { return entry_; }
};

class Receipt {
    string ticketId_;
    Money fee_;
    TimePoint exit_;
public:
    Receipt(string ticketId, Money fee, TimePoint exit) : ticketId_(std::move(ticketId)), fee_(fee), exit_(exit) {}
    const string& ticketId() const { return ticketId_; }
    Money fee() const { return fee_; }
    TimePoint exitTime() const { return exit_; }
};

class IdGenerator {
    atomic<uint64_t> seq{1};
public:
    string next(const string& prefix) { return prefix + to_string(seq++); }
};

class TicketFactory {
    IdGenerator& gen_;
public:
    explicit TicketFactory(IdGenerator& g) : gen_(g) {}
    Ticket* create(const Vehicle& v, Spot* s) {
        return new Ticket(gen_.next("T-"), v.plate(), s, Clock::now());
    }
};

class ReceiptFactory {
public:
    Receipt* create(const Ticket& t, Money m) {
        return new Receipt(t.id(), m, Clock::now());
    }
};

// --------------- ParkingLot (Subject) ---------------
class ParkingLot {
    vector<Floor*> floors_;            // ParkingLot owns floors
    vector<LotObserver*> observers_;   // external ownership for observers
    AllocationStrategy* alloc_;        // injected, external ownership
    PricingStrategy* pricing_;         // injected, external ownership
    TicketFactory ticketFactory_;
    ReceiptFactory receiptFactory_;
    unordered_map<string, Ticket*> active_; // ticketId -> ticket (owned)
    mutable mutex mtx_;

    void notifyIfNeeded() {
        size_t total=0, used=0;
        for (auto* f : floors_) { total += f->capacity(); used += f->used(); }
        double util = total ? (double)used / total : 0.0;
        if (util >= 0.9 || util <= 0.1) {
            LotEvent e;
            e.utilization = util;
            e.message = util >= 0.9 ? "Lot 90%+ full" : "Lot 10%- empty";
            for (auto* obs : observers_) obs->onEvent(e);
        }
    }
public:
    ParkingLot(AllocationStrategy* alloc,
               PricingStrategy* pricing,
               IdGenerator& idGen)
        : alloc_(alloc), pricing_(pricing), ticketFactory_(idGen) {}

    ~ParkingLot() {
        // delete owned tickets
        for (auto& kv : active_) delete kv.second;
        active_.clear();
        // delete owned floors and their spots handled by Floor destructor
        for (auto* f : floors_) delete f;
        floors_.clear();
    }

    Floor* addFloor(Floor* f) {
        floors_.push_back(f);
        return f;
    }
    void addObserver(LotObserver* o) { observers_.push_back(o); }
    void removeObserver(LotObserver* o) {
        observers_.erase(remove(observers_.begin(), observers_.end(), o), observers_.end());
    }

    Ticket* enter(const Vehicle& v) {
        lock_guard<mutex> lock(mtx_);
        vector<Floor*> fl = floors_;

        Spot* s = alloc_->allocate(fl, v);
        if (!s) return nullptr;
        s->occupy();
        Ticket* t = ticketFactory_.create(v, s);
        active_[t->id()] = t;
        notifyIfNeeded();
        return t;
    }

    Receipt* exit(const string& ticketId, const Vehicle& v) {
        lock_guard<mutex> lock(mtx_);
        auto it = active_.find(ticketId);
        if (it == active_.end()) return nullptr;
        Ticket* t = it->second;
        TimePoint now = Clock::now();
        Money fee = pricing_->computeFee(t->entryTime(), now, v, t->spot()->spotType());
        t->spot()->release();
        Receipt* receipt = receiptFactory_.create(*t, fee);
        delete t;
        active_.erase(it);
        notifyIfNeeded();
        return receipt;
    }
};

// --------------- Example Observer ---------------
class ConsoleObserver : public LotObserver {
public:
    void onEvent(const LotEvent& e) override {
        cout << "[LotEvent] " << e.message << " utilization=" << e.utilization << "\n";
    }
};

// --------------- Demo main ---------------
int main() {
    // Strategies (lifetime external to ParkingLot)
    FirstFitAllocation alloc;
    ProgressivePricing pricing;
    IdGenerator idGen;

    ParkingLot lot(&alloc, &pricing, idGen);
    ConsoleObserver obs;
    lot.addObserver(&obs);

    // Build floor and spots; ParkingLot owns floor; Floor owns spots
    Floor* f1 = new Floor("F1");
    for (int i=0;i<5;++i) f1->addSpot(new Spot("F1-M-"+to_string(i), SpotType::Motorcycle));
    for (int i=0;i<10;++i) f1->addSpot(new Spot("F1-C-"+to_string(i), SpotType::Compact));
    for (int i=0;i<4;++i) f1->addSpot(new Spot("F1-L-"+to_string(i), SpotType::Large));
    for (int i=0;i<3;++i) f1->addSpot(new Spot("F1-EV-"+to_string(i), SpotType::EV));

    lot.addFloor(f1);

    CarV car("CAR-123");
    EVV ev("EV-999");
    MotorcycleV moto("MOTO-7");
    TruckV truck("TRK-55");

    Ticket* t1 = lot.enter(car);
    Ticket* t2 = lot.enter(ev);
    Ticket* t3 = lot.enter(moto);
    Ticket* t4 = lot.enter(truck);

    if (!t1 || !t2 || !t3 || !t4) {
        cout << "Allocation failed for some vehicle\n";
        // ParkingLot destructor cleans owned floors and tickets
        return 0;
    }

    this_thread::sleep_for(chrono::milliseconds(1500));

    Receipt* r1 = lot.exit(t1->id(), car);
    Receipt* r2 = lot.exit(t2->id(), ev);
    Receipt* r3 = lot.exit(t3->id(), moto);
    Receipt* r4 = lot.exit(t4->id(), truck);

    cout << fixed << setprecision(2);
    if (r1) { cout << "Receipt " << r1->ticketId() << " fee $" << r1->fee().dollars() << "\n"; delete r1; }
    if (r2) { cout << "Receipt " << r2->ticketId() << " fee $" << r2->fee().dollars() << "\n"; delete r2; }
    if (r3) { cout << "Receipt " << r3->ticketId() << " fee $" << r3->fee().dollars() << "\n"; delete r3; }
    if (r4) { cout << "Receipt " << r4->ticketId() << " fee $" << r4->fee().dollars() << "\n"; delete r4; }

    return 0;
}