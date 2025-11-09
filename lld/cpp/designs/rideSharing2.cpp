#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <stdexcept>

using namespace std;

// -----------------------------
// Vehicle
// -----------------------------
class Vehicle {
private:
    string vehicleName;
    string licensePlate;
public:
    Vehicle(const string &name, const string &plate)
        : vehicleName(name), licensePlate(plate) {}

    string getVehicleName() const { return vehicleName; }
    string getLicensePlate() const { return licensePlate; }
};

// -----------------------------
// Vehicle Factory
// -----------------------------
class VehicleFactory {
public:
    static shared_ptr<Vehicle> createVehicle(const string &type, const string &name, const string &plate) {
        // Extendable for different vehicle types
        return make_shared<Vehicle>(name, plate);
    }
};

// -----------------------------
// User
// -----------------------------
class User {
private:
    string name;
    char gender;
    int age;
    vector<shared_ptr<Vehicle>> vehicles;
    int ridesTaken = 0;
    int ridesOffered = 0;
public:
    User(const string &n, char g, int a) : name(n), gender(g), age(a) {}

    string getName() const { return name; }

    void addVehicle(shared_ptr<Vehicle> vehicle) { vehicles.push_back(vehicle); }
    const vector<shared_ptr<Vehicle>>& getVehicles() const { return vehicles; }

    void incrementRidesTaken() { ridesTaken++; }
    void incrementRidesOffered() { ridesOffered++; }
    int getRidesTaken() const { return ridesTaken; }
    int getRidesOffered() const { return ridesOffered; }
};

// -----------------------------
// Ride
// -----------------------------
class Ride {
private:
    int rideId;
    shared_ptr<User> driver;
    shared_ptr<Vehicle> vehicle;
    string origin;
    string destination;
    int availableSeats;
    bool active = true;
    vector<shared_ptr<User>> passengers;
public:
    Ride(int id, shared_ptr<User> d, shared_ptr<Vehicle> v,
         const string &o, const string &dest, int seats)
         : rideId(id), driver(d), vehicle(v), origin(o), destination(dest), availableSeats(seats) {}

    int getRideId() const { return rideId; }
    shared_ptr<User> getDriver() const { return driver; }
    shared_ptr<Vehicle> getVehicle() const { return vehicle; }
    string getOrigin() const { return origin; }
    string getDestination() const { return destination; }
    int getAvailableSeats() const { return availableSeats; }
    bool isActive() const { return active; }

    void addPassenger(shared_ptr<User> user, int seats) {
        if(seats > availableSeats) throw runtime_error("Not enough seats");
        passengers.push_back(user);
        availableSeats -= seats;
    }

    void endRide() { active = false; }
};

// -----------------------------
// Strategy Pattern for Ride Selection
// -----------------------------
class RideSelectionStrategy {
public:
    virtual shared_ptr<Ride> select(const vector<shared_ptr<Ride>> &candidates) = 0;
    virtual ~RideSelectionStrategy() {}
};

class MostVacantStrategy : public RideSelectionStrategy {
public:
    shared_ptr<Ride> select(const vector<shared_ptr<Ride>> &candidates) override {
        if(candidates.empty()) return nullptr;
        return *max_element(candidates.begin(), candidates.end(),
            [](shared_ptr<Ride> a, shared_ptr<Ride> b){ return a->getAvailableSeats() < b->getAvailableSeats(); });
    }
};

class PreferredVehicleStrategy : public RideSelectionStrategy {
private:
    string preferredVehicle;
public:
    PreferredVehicleStrategy(const string &vehicle) : preferredVehicle(vehicle) {}
    shared_ptr<Ride> select(const vector<shared_ptr<Ride>> &candidates) override {
        for(auto &ride : candidates) {
            if(ride->getVehicle()->getVehicleName() == preferredVehicle)
                return ride;
        }
        return nullptr;
    }
};

// -----------------------------
// Ride Sharing System (Singleton)
// -----------------------------
class RideSharingSystem {
private:
    static shared_ptr<RideSharingSystem> instance;

    map<string, shared_ptr<User>> users;
    map<int, shared_ptr<Ride>> rides;
    int nextRideId = 1;

    RideSharingSystem() {} // private constructor

public:
    static shared_ptr<RideSharingSystem> getInstance() {
        if(!instance) {
            instance = shared_ptr<RideSharingSystem>(new RideSharingSystem());
        }
        return instance;
    }

    void addUser(const string &name, char gender, int age) {
        if(users.find(name) != users.end()) {
            cout << "User " << name << " already exists!\n";
            return;
        }
        users[name] = make_shared<User>(name, gender, age);
    }

    void addVehicle(const string &userName, const string &vehicleName, const string &licensePlate) {
        auto it = users.find(userName);
        if(it == users.end()) {
            cout << "User " << userName << " not found!\n";
            return;
        }
        it->second->addVehicle(VehicleFactory::createVehicle(vehicleName, vehicleName, licensePlate));
    }

    bool offerRide(const string &userName, const string &vehiclePlate, const string &origin, const string &destination, int seats) {
        auto it = users.find(userName);
        if(it == users.end()) return false;

        auto user = it->second;

        shared_ptr<Vehicle> vehicle = nullptr;
        for(auto &v : user->getVehicles()) {
            if(v->getLicensePlate() == vehiclePlate) {
                vehicle = v;
                break;
            }
        }
        if(!vehicle) {
            cout << "Vehicle not found for user " << userName << endl;
            return false;
        }

        // Check if already offering active ride with this vehicle
        for(auto &r : rides) {
            auto ride = r.second;
            if(ride->getDriver() == user && ride->getVehicle()->getLicensePlate() == vehiclePlate && ride->isActive()) {
                cout << "Ride already offered for this vehicle.\n";
                return false;
            }
        }

        auto ride = make_shared<Ride>(nextRideId++, user, vehicle, origin, destination, seats);
        rides[ride->getRideId()] = ride;
        user->incrementRidesOffered();
        return true;
    }

    bool selectRide(const string &userName, const string &origin, const string &destination, int seats, shared_ptr<RideSelectionStrategy> strategy) {
        auto it = users.find(userName);
        if(it == users.end()) return false;
        auto user = it->second;

        vector<shared_ptr<Ride>> candidates;
        for(auto &r : rides) {
            auto ride = r.second;
            if(ride->isActive() && ride->getOrigin() == origin && ride->getDestination() == destination && ride->getAvailableSeats() >= seats) {
                candidates.push_back(ride);
            }
        }

        if(candidates.empty()) {
            cout << "No rides found\n";
            return false;
        }

        auto chosenRide = strategy->select(candidates);
        if(!chosenRide) {
            cout << "No rides found matching strategy\n";
            return false;
        }

        chosenRide->addPassenger(user, seats);
        user->incrementRidesTaken();
        return true;
    }

    void endRide(int rideId) {
        auto it = rides.find(rideId);
        if(it == rides.end()) {
            cout << "Ride not found\n";
            return;
        }
        it->second->endRide();
    }

    void printRideStats() {
        for(auto &u : users) {
            cout << u.second->getName() << ": " << u.second->getRidesTaken() << " Taken, "
                 << u.second->getRidesOffered() << " Offered\n";
        }
    }
};
shared_ptr<RideSharingSystem> RideSharingSystem::instance = nullptr;

// -----------------------------
// Demo Driver
// -----------------------------
int main() {
    auto system = RideSharingSystem::getInstance();

    system->addUser("Rohan",'M',36);
    system->addUser("Shashank",'M',29);
    system->addUser("Nandini",'F',29);
    system->addUser("Shipra",'F',27);
    system->addUser("Gaurav",'M',29);
    system->addUser("Rahul",'M',35);

    system->addVehicle("Rohan","Swift","KA-01-12345");
    system->addVehicle("Shashank","Baleno","TS-05-62395");
    system->addVehicle("Shipra","Polo","KA-05-41491");
    system->addVehicle("Shipra","Activa","KA-12-12332");
    system->addVehicle("Rahul","XUV","KA-05-1234");

    system->offerRide("Rohan","KA-01-12345","Hyderabad","Bangalore",1);
    system->offerRide("Shipra","KA-12-12332","Bangalore","Mysore",1);
    system->offerRide("Shipra","KA-05-41491","Bangalore","Mysore",2);
    system->offerRide("Shashank","TS-05-62395","Hyderabad","Bangalore",2);
    system->offerRide("Rahul","KA-05-1234","Hyderabad","Bangalore",5);
    system->offerRide("Rohan","KA-01-12345","Bangalore","Pune",1); // should fail

    // Select rides using Strategy
    system->selectRide("Nandini","Bangalore","Mysore",1, make_shared<MostVacantStrategy>());
    system->selectRide("Gaurav","Bangalore","Mysore",1, make_shared<PreferredVehicleStrategy>("Activa"));
    system->selectRide("Shashank","Mumbai","Bangalore",1, make_shared<MostVacantStrategy>()); // no rides
    system->selectRide("Rohan","Hyderabad","Bangalore",1, make_shared<PreferredVehicleStrategy>("Baleno")); // no rides
    system->selectRide("Shashank","Hyderabad","Bangalore",1, make_shared<PreferredVehicleStrategy>("Polo")); // no rides

    // End rides
    system->endRide(1);
    system->endRide(2);
    system->endRide(3);
    system->endRide(4);

    // Print stats
    system->printRideStats();

    return 0;
}