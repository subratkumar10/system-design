#include <bits/stdc++.h>
#include <atomic>
#include <mutex>
#include <thread>
using namespace std;

// -------------------- Utilities --------------------
struct Location {
    int x;
    int y;
    Location(int x_=0, int y_=0): x(x_), y(y_) {}
    int distance(const Location &other) const {
        return abs(x - other.x) + abs(y - other.y); // Manhattan distance
    }
    string toString() const {
        return "(" + to_string(x) + "," + to_string(y) + ")";
    }
};

// -------------------- Domain Entities --------------------
enum class DriverStatus { OFFLINE, AVAILABLE, ON_RIDE };
enum class RideStatus { REQUESTED, ACCEPTED, ONGOING, COMPLETED, CANCELLED };

struct Driver {
    int id;
    string name;
    Location loc;
    DriverStatus status;
    Driver(int id_, string name_, Location loc_): id(id_), name(move(name_)), loc(loc_), status(DriverStatus::AVAILABLE) {}
};

struct Rider {
    int id;
    string name;
    Location loc;
    Rider(int id_, string name_, Location loc_): id(id_), name(move(name_)), loc(loc_) {}
};

struct Ride {
    int id;
    int riderId;
    int driverId; // -1 if not assigned
    Location from;
    Location to;
    RideStatus status;
    long long startTs;
    long long endTs;
    double fare;

    Ride(int id_, int riderId_, Location f, Location t)
      : id(id_), riderId(riderId_), driverId(-1), from(f), to(t),
        status(RideStatus::REQUESTED), startTs(0), endTs(0), fare(0.0) {}
};

// -------------------- Fare Calculator --------------------
class FareCalculator {
public:
    static double calculate(const Location &from, const Location &to) {
        int dist = from.distance(to);
        double base = 5.0;               // base fare
        double perUnit = 1.5;            // per unit distance
        return base + perUnit * dist;
    }
};

// -------------------- Ride Manager (Core) --------------------
class RideManager {
private:
    atomic<int> nextDriverId{1};
    atomic<int> nextRiderId{1};
    atomic<int> nextRideId{1};

    unordered_map<int, shared_ptr<Driver>> drivers;
    unordered_map<int, shared_ptr<Rider>> riders;
    unordered_map<int, shared_ptr<Ride>> rides;

    // quick lookup of available drivers (IDs)
    // we keep drivers map authoritative; availability checked under mutex
    mutex mu;

public:
    // register driver
    int registerDriver(const string &name, const Location &loc) {
        int id = nextDriverId++;
        auto d = make_shared<Driver>(id, name, loc);
        lock_guard<mutex> lock(mu);
        drivers[id] = d;
        return id;
    }

    // register rider
    int registerRider(const string &name, const Location &loc) {
        int id = nextRiderId++;
        auto r = make_shared<Rider>(id, name, loc);
        lock_guard<mutex> lock(mu);
        riders[id] = r;
        return id;
    }

    // update driver location & status
    void updateDriverLocation(int driverId, const Location &loc) {
        lock_guard<mutex> lock(mu);
        auto it = drivers.find(driverId);
        if(it != drivers.end()) it->second->loc = loc;
    }

    void setDriverStatus(int driverId, DriverStatus status) {
        lock_guard<mutex> lock(mu);
        auto it = drivers.find(driverId);
        if(it != drivers.end()) it->second->status = status;
    }

    // Create ride request; returns rideId
    int requestRide(int riderId, const Location &from, const Location &to) {
        int rid = nextRideId++;
        auto ride = make_shared<Ride>(rid, riderId, from, to);
        {
            lock_guard<mutex> lock(mu);
            rides[rid] = ride;
        }
        // attempt to match immediately (synchronous match for simplicity)
        matchDriverForRide(rid);
        return rid;
    }

    // Matching strategy: find nearest AVAILABLE driver
    bool matchDriverForRide(int rideId) {
        lock_guard<mutex> lock(mu);
        auto rIt = rides.find(rideId);
        if(rIt == rides.end()) return false;
        auto ride = rIt->second;
        if(ride->status != RideStatus::REQUESTED) return false;

        int bestDriverId = -1;
        int bestDist = INT_MAX;
        for(auto &p: drivers) {
            auto &d = p.second;
            if(d->status != DriverStatus::AVAILABLE) continue;
            int dist = d->loc.distance(ride->from);
            if(dist < bestDist) {
                bestDist = dist;
                bestDriverId = d->id;
            }
        }

        if(bestDriverId == -1) {
            // no driver now; keep in REQUESTED state
            return false;
        }

        // assign
        auto driver = drivers[bestDriverId];
        driver->status = DriverStatus::ON_RIDE;
        ride->driverId = bestDriverId;
        ride->status = RideStatus::ACCEPTED;
        // compute fare upfront
        ride->fare = FareCalculator::calculate(ride->from, ride->to);
        return true;
    }

    // driver starts ride (simulate driver arrival & start)
    bool startRide(int rideId) {
        lock_guard<mutex> lock(mu);
        auto it = rides.find(rideId);
        if(it == rides.end()) return false;
        auto ride = it->second;
        if(ride->status != RideStatus::ACCEPTED) return false;
        ride->status = RideStatus::ONGOING;
        ride->startTs = time(nullptr);
        return true;
    }

    // finish ride
    bool completeRide(int rideId) {
        lock_guard<mutex> lock(mu);
        auto it = rides.find(rideId);
        if(it == rides.end()) return false;
        auto ride = it->second;
        if(ride->status != RideStatus::ONGOING) return false;
        ride->status = RideStatus::COMPLETED;
        ride->endTs = time(nullptr);
        // free driver
        int did = ride->driverId;
        auto dit = drivers.find(did);
        if(dit != drivers.end()) {
            dit->second->status = DriverStatus::AVAILABLE;
            dit->second->loc = ride->to; // driver moves to destination
        }
        return true;
    }

    // cancel ride
    bool cancelRide(int rideId) {
        lock_guard<mutex> lock(mu);
        auto it = rides.find(rideId);
        if(it == rides.end()) return false;
        auto ride = it->second;
        if(ride->status == RideStatus::COMPLETED || ride->status == RideStatus::CANCELLED) return false;
        ride->status = RideStatus::CANCELLED;
        // free driver if assigned
        if(ride->driverId != -1) {
            auto dit = drivers.find(ride->driverId);
            if(dit != drivers.end()) dit->second->status = DriverStatus::AVAILABLE;
        }
        return true;
    }

    // query methods
    shared_ptr<Ride> getRide(int rideId) {
        lock_guard<mutex> lock(mu);
        auto it = rides.find(rideId);
        return it != rides.end() ? it->second : nullptr;
    }

    vector<int> availableDrivers() {
        lock_guard<mutex> lock(mu);
        vector<int> res;
        for(auto &p: drivers) if(p.second->status == DriverStatus::AVAILABLE) res.push_back(p.first);
        return res;
    }

    void printStatus() {
        lock_guard<mutex> lock(mu);
        cout << "=== System Status ===\n";
        cout << "Drivers:\n";
        for(auto &p: drivers) {
            auto &d = p.second;
            cout << "  D#" << d->id << " " << d->name << " loc=" << d->loc.toString()
                 << " status=" << (d->status==DriverStatus::AVAILABLE ? "AVL" : d->status==DriverStatus::ON_RIDE ? "ON" : "OFF") << "\n";
        }
        cout << "Rides:\n";
        for(auto &p: rides) {
            auto &r = p.second;
            cout << "  R#" << r->id << " rider=" << r->riderId << " driver=" << r->driverId
                 << " from="<< r->from.toString() << " to="<< r->to.toString()
                 << " status=" << (r->status==RideStatus::REQUESTED? "REQ": r->status==RideStatus::ACCEPTED?"ACC": r->status==RideStatus::ONGOING?"ONG":"CMP")
                 << " fare=" << r->fare << "\n";
        }
        cout << "=====================\n";
    }
};

// -------------------- Simple Demo --------------------
int main() {
    RideManager rm;

    // register drivers and riders
    int d1 = rm.registerDriver("Alice", Location(0,0));
    int d2 = rm.registerDriver("Bob", Location(5,5));
    int d3 = rm.registerDriver("Charlie", Location(2,1));
    int r1 = rm.registerRider("Rita", Location(1,0));
    int r2 = rm.registerRider("Sam", Location(10,10));

    rm.printStatus();

    // Rider 1 requests a nearby ride
    int ride1 = rm.requestRide(r1, Location(1,0), Location(4,4));
    cout << "Ride requested id="<<ride1<<"\n";
    rm.printStatus();

    // Start ride if accepted
    if(rm.getRide(ride1) && rm.getRide(ride1)->driverId != -1) {
        rm.startRide(ride1);
        cout << "Ride started: " << ride1 << "\n";
    }
    rm.printStatus();

    // Simulate finish
    this_thread::sleep_for(chrono::milliseconds(200));
    rm.completeRide(ride1);
    cout << "Ride completed: " << ride1 << "\n";
    rm.printStatus();

    // Rider 2 requests a ride (far away) - may not find driver if all busy
    int ride2 = rm.requestRide(r2, Location(10,10), Location(12,12));
    cout << "Ride requested id="<<ride2<<"\n";
    rm.printStatus();

    return 0;
}