#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <cmath>

// Basic aliases
typedef int LiftId;
typedef int Floor;

// =========================== Direction and Door as ints ===========================
struct Direction {
    static int Up() { return 1; }
    static int Down() { return -1; }
    static int Idle() { return 0; }
};

struct DoorState {
    static int Closed() { return 0; }
    static int Open() { return 1; }
};

// =========================== Requests (Command-like) ==============================
struct HallCall {
    Floor atFloor;
    int direction;  // Direction::Up or Direction::Down
    HallCall(): atFloor(0), direction(Direction::Idle()) {}
    HallCall(Floor f, int dir): atFloor(f), direction(dir) {}
};

struct CarCall {
    LiftId lift;
    Floor destination;
    CarCall(): lift(-1), destination(0) {}
    CarCall(LiftId id, Floor dest): lift(id), destination(dest) {}
};

// =========================== Observer (events) ====================================
class ElevatorObserver {
public:
    virtual ~ElevatorObserver() {}
    virtual void onAssign(LiftId lift, Floor hallFloor, int dir) = 0;
    virtual void onArrive(LiftId lift, Floor floor) = 0;
    virtual void onDoor(LiftId lift, int doorState) = 0;
};

// =========================== Lift (State Machine) =================================
class Lift {
public:
    LiftId id;
    Floor currentFloor;
    int direction;        // -1,0,1
    int capacity;
    int passengers;
    int door;             // 0=Closed, 1=Open
    int minFloor, maxFloor;

    std::vector<Floor> upQueue;    // ascending
    std::vector<Floor> downQueue;  // descending

    Lift(): id(-1), currentFloor(0), direction(Direction::Idle()),
            capacity(10), passengers(0), door(DoorState::Closed()),
            minFloor(0), maxFloor(20) {}

    Lift(LiftId i, Floor start, int minF, int maxF, int cap)
        : id(i), currentFloor(start), direction(Direction::Idle()),
          capacity(cap), passengers(0), door(DoorState::Closed()),
          minFloor(minF), maxFloor(maxF) {}

    bool canServe(Floor f) const {
        return f >= minFloor && f <= maxFloor;
    }

    void addDestination(Floor dest) {
        if (dest == currentFloor) {
            // If at the floor, open doors immediately on next tick
            if (direction == Direction::Idle()) {
                // no-op here; step will open when arrival occurs
            }
            return;
        }
        if (dest > currentFloor) {
            if (!contains(upQueue, dest)) insertSortedAsc(upQueue, dest);
            if (direction == Direction::Idle()) direction = Direction::Up();
        } else {
            if (!contains(downQueue, dest)) insertSortedDesc(downQueue, dest);
            if (direction == Direction::Idle()) direction = Direction::Down();
        }
    }

    // One simulation step: move/arrive/open-close doors
    void step(ElevatorObserver* obs) {
        // If doors are open, close them this tick and reassess direction
        if (door == DoorState::Open()) {
            door = DoorState::Closed();
            if (obs) obs->onDoor(id, door);
            setNextDirectionIfNeeded();
            return;
        }

        if (direction == Direction::Up()) {
            if (currentFloor < maxFloor) currentFloor++;
            if (obs) obs->onArrive(id, currentFloor);

            if (popMatch(upQueue, currentFloor)) {
                arriveStop(obs);
            } else if (upQueue.empty()) {
                if (!downQueue.empty()) direction = Direction::Down();
                else direction = Direction::Idle();
            }
        } else if (direction == Direction::Down()) {
            if (currentFloor > minFloor) currentFloor--;
            if (obs) obs->onArrive(id, currentFloor);

            if (popMatch(downQueue, currentFloor)) {
                arriveStop(obs);
            } else if (downQueue.empty()) {
                if (!upQueue.empty()) direction = Direction::Up();
                else direction = Direction::Idle();
            }
        } else {
            // Idle: nothing to do
        }
    }

    // Heuristic cost function for dispatcher
    int estimateCost(Floor requestFloor, int requestDir) const {
        if (!canServe(requestFloor)) return 1000000000;
        int dist = std::abs(currentFloor - requestFloor);

        if (direction == Direction::Idle()) return dist;

        if (direction == Direction::Up()) {
            bool onTheWay = (requestFloor >= currentFloor);
            if (onTheWay && requestDir == Direction::Up()) return dist;
            return dist + 5 + (int)upQueue.size() + (int)downQueue.size();
        } else if (direction == Direction::Down()) {
            bool onTheWay = (requestFloor <= currentFloor);
            if (onTheWay && requestDir == Direction::Down()) return dist;
            return dist + 5 + (int)upQueue.size() + (int)downQueue.size();
        }
        return dist + 10;
    }

private:
    static bool contains(const std::vector<Floor>& v, Floor f) {
        for (size_t i=0;i<v.size();++i) if (v[i]==f) return true;
        return false;
    }
    static void insertSortedAsc(std::vector<Floor>& v, Floor f) {
        size_t i=0; while (i<v.size() && v[i] < f) ++i;
        v.insert(v.begin()+i, f);
    }
    static void insertSortedDesc(std::vector<Floor>& v, Floor f) {
        size_t i=0; while (i<v.size() && v[i] > f) ++i;
        v.insert(v.begin()+i, f);
    }
    static bool popMatch(std::vector<Floor>& v, Floor f) {
        for (size_t i=0;i<v.size();++i) if (v[i]==f) { v.erase(v.begin()+i); return true; }
        return false;
    }

    void arriveStop(ElevatorObserver* obs) {
        // Open doors on arrival to a requested floor
        door = DoorState::Open();
        if (obs) obs->onDoor(id, door);
        // Boarding/alighting logic omitted; could update passengers here
    }

    void setNextDirectionIfNeeded() {
        if (direction == Direction::Idle()) {
            if (!upQueue.empty() && !downQueue.empty()) {
                int upDist = std::abs(currentFloor - upQueue.front());
                int downDist = std::abs(currentFloor - downQueue.front());
                direction = (upDist <= downDist) ? Direction::Up() : Direction::Down();
            } else if (!upQueue.empty()) {
                direction = Direction::Up();
            } else if (!downQueue.empty()) {
                direction = Direction::Down();
            }
        }
    }
};

// =========================== Dispatcher (Strategy) ==========================
class DispatchStrategy {
public:
    virtual ~DispatchStrategy() {}
    virtual LiftId chooseLift(const std::vector<Lift*>& lifts, const HallCall& call) = 0;
};

// Nearest car with direction bias
class NearestDirectionAware : public DispatchStrategy {
public:
    LiftId chooseLift(const std::vector<Lift*>& lifts, const HallCall& call) override {
        int bestCost = std::numeric_limits<int>::max();
        LiftId bestId = -1;
        for (size_t i=0;i<lifts.size();++i) {
            Lift* lf = lifts[i];
            if (!lf) continue;
            int cost = lf->estimateCost(call.atFloor, call.direction);
            if (cost < bestCost) {
                bestCost = cost;
                bestId = lf->id;
            }
        }
        return bestId;
    }
};

// =========================== Controller implements Observer ========================
// Facade + Observer: orchestrates lifts and forwards events to external observers
class ElevatorController : public ElevatorObserver {
public:
    std::vector<Lift*> lifts;                 // owns Lift*
    DispatchStrategy* strategy;               // external lifetime
    std::vector<ElevatorObserver*> observers; // external lifetime

    ElevatorController(): strategy(0) {}
    ~ElevatorController() {
        for (size_t i=0;i<lifts.size();++i) delete lifts[i];
        lifts.clear();
    }

    void addObserver(ElevatorObserver* o) { observers.push_back(o); }
    void addLift(Lift* l) { lifts.push_back(l); }

    // ElevatorObserver implementation: forward to external observers
    void onAssign(LiftId lift, Floor hallFloor, int dir) override {
        for (size_t i=0;i<observers.size();++i) observers[i]->onAssign(lift, hallFloor, dir);
    }
    void onArrive(LiftId lift, Floor floor) override {
        for (size_t i=0;i<observers.size();++i) observers[i]->onArrive(lift, floor);
    }
    void onDoor(LiftId lift, int doorState) override {
        for (size_t i=0;i<observers.size();++i) observers[i]->onDoor(lift, doorState);
    }

    // Requests
    LiftId requestPickup(const HallCall& call) {
        if (!strategy) return -1;
        LiftId id = strategy->chooseLift(lifts, call);
        Lift* l = getLiftById(id);
        if (l) {
            l->addDestination(call.atFloor);
            onAssign(id, call.atFloor, call.direction); // notify observers
        }
        return id;
    }

    void requestDropoff(const CarCall& call) {
        Lift* l = getLiftById(call.lift);
        if (l) l->addDestination(call.destination);
    }

    // Advance simulation
    void stepAll() {
        for (size_t i=0;i<lifts.size();++i) {
            lifts[i]->step(this); // valid: controller is an ElevatorObserver
        }
    }

private:
    Lift* getLiftById(LiftId id) {
        for (size_t i=0;i<lifts.size();++i) if (lifts[i]->id == id) return lifts[i];
        return 0;
    }
};

// =========================== Console Observer ======================================
class ConsoleObserver : public ElevatorObserver {
public:
    void onAssign(LiftId lift, Floor floor, int dir) override {
        std::cout << "[Assign] Lift " << lift << " to floor " << floor
                  << " dir " << (dir > 0 ? "Up" : "Down") << "\n";
    }
    void onArrive(LiftId lift, Floor floor) override {
        std::cout << "[Arrive] Lift " << lift << " at floor " << floor << "\n";
    }
    void onDoor(LiftId lift, int doorState) override {
        std::cout << "[Door] Lift " << lift << " " << (doorState ? "Open" : "Closed") << "\n";
    }
};

// =========================== Simulation Main =======================================
int main() {
    ElevatorController controller;
    ConsoleObserver obs;
    controller.addObserver(&obs);

    NearestDirectionAware strategy;
    controller.strategy = &strategy;

    // Create lifts (owned by controller)
    controller.addLift(new Lift(0, 0, 0, 20, 10));
    controller.addLift(new Lift(1, 10, 0, 20, 10));
    controller.addLift(new Lift(2, 5, 0, 20, 12));

    // Simulate requests
    // User at floor 7 wants to go up
    LiftId l1 = controller.requestPickup(HallCall(7, Direction::Up()));
    controller.requestDropoff(CarCall(l1, 15));

    // Another user at floor 3 wants to go down to 0
    LiftId l2 = controller.requestPickup(HallCall(3, Direction::Down()));
    controller.requestDropoff(CarCall(l2, 0));

    // Run N ticks
    for (int t = 0; t < 30; ++t) {
        controller.stepAll();
    }

    return 0;
}