#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
//patterns (State, Strategy, Command, Observer)
// Basic type
typedef int Floor;

struct Direction {
    static int Up() { return 1; }
    static int Down() { return -1; }
    static int Idle() { return 0; }
};

// ===================== Observer Pattern =====================
class ElevatorObserver {
public:
    virtual ~ElevatorObserver() {}
    virtual void onArrive(Floor floor) = 0;
    virtual void onDoorChange(int doorOpen) = 0; // 1=open, 0=closed
    virtual void onRequestQueued(Floor at, const char* type) = 0;
};

class ConsoleObserver : public ElevatorObserver {
public:
    void onArrive(Floor floor) override {
        std::cout << "[Arrive] Floor " << floor << "\n";
    }
    void onDoorChange(int doorOpen) override {
        std::cout << "[Door] " << (doorOpen ? "Open" : "Closed") << "\n";
    }
    void onRequestQueued(Floor at, const char* type) override {
        std::cout << "[Queue] " << type << " at floor " << at << "\n";
    }
};

// ===================== Command Pattern ======================
struct HallCall {
    Floor atFloor;
    int direction;
    HallCall(): atFloor(0), direction(Direction::Idle()) {}
    HallCall(Floor f, int dir): atFloor(f), direction(dir) {}
};
struct CarCall {
    Floor destination;
    CarCall(): destination(0) {}
    CarCall(Floor f): destination(f) {}
};

// ===================== Strategy Pattern =====================
class StopSelectionStrategy {
public:
    virtual ~StopSelectionStrategy() {}
    virtual int chooseNextDirection(Floor current,
                                    const std::vector<Floor>& upQueue,
                                    const std::vector<Floor>& downQueue) = 0;
};

class NearestStopStrategy : public StopSelectionStrategy {
public:
    int chooseNextDirection(Floor current,
                            const std::vector<Floor>& upQueue,
                            const std::vector<Floor>& downQueue) override {
        bool hasUp = !upQueue.empty();
        bool hasDown = !downQueue.empty();
        if (!hasUp && !hasDown) return Direction::Idle();
        if (hasUp && !hasDown) return Direction::Up();
        if (!hasUp && hasDown) return Direction::Down();
        int upDist = std::abs(current - upQueue.front());
        int downDist = std::abs(current - downQueue.front());
        return (upDist <= downDist) ? Direction::Up() : Direction::Down();
    }
};

// ===================== State Pattern Interfaces =====================
class Elevator; // forward

class DoorState {
public:
    virtual ~DoorState() {}
    virtual void onEnter(Elevator& e) = 0;
    virtual void onTick(Elevator& e) = 0;
    virtual const char* name() const = 0;
};

class MovementState {
public:
    virtual ~MovementState() {}
    virtual void onEnter(Elevator& e) = 0;
    virtual void onTick(Elevator& e) = 0;
    virtual const char* name() const = 0;
};

// ===================== Elevator (Context, no friends) =====================
class Elevator {
public:
    // Construction
    Elevator(Floor start, int minF, int maxF, int cap,
             StopSelectionStrategy* strategy,
             ElevatorObserver* obs)
        : currentFloor_(start),
          minFloor_(minF), maxFloor_(maxF),
          capacity_(cap), passengers_(0),
          stopStrategy_(strategy), observer_(obs),
          doorOpenState_(0), doorClosedState_(0),
          movingUpState_(0), movingDownState_(0), idleState_(0),
          doorState_(0), moveState_(0) {
        initStates();
        setDoorState(doorClosedState_); // closed to start
        setMoveState(idleState_);       // idle to start
    }

    ~Elevator() {
        delete doorOpenState_;
        delete doorClosedState_;
        delete movingUpState_;
        delete movingDownState_;
        delete idleState_;
    }

    // Commands
    void requestHall(const HallCall& call) {
        if (!inRange(call.atFloor)) return;
        enqueueStop(call.atFloor);
        if (observer_) observer_->onRequestQueued(call.atFloor, "Hall");
    }
    void requestCar(const CarCall& call) {
        if (!inRange(call.destination)) return;
        enqueueStop(call.destination);
        if (observer_) observer_->onRequestQueued(call.destination, "Car");
    }

    // Simulation tick
    void step() {
        doorState_->onTick(*this);
        moveState_->onTick(*this);
    }

    bool isIdle() const {
        return (moveState_ == idleState_) && upQueue_.empty() && downQueue_.empty();
    }

    // Public APIs used by states (no friends needed)
    // Queries
    Floor currentFloor() const { return currentFloor_; }
    int minFloor() const { return minFloor_; }
    int maxFloor() const { return maxFloor_; }
    bool hasUpStops() const { return !upQueue_.empty(); }
    bool hasDownStops() const { return !downQueue_.empty(); }
    bool atUpStop() const { return contains(upQueue_, currentFloor_); }
    bool atDownStop() const { return contains(downQueue_, currentFloor_); }
    ElevatorObserver* observer() const { return observer_; }
    StopSelectionStrategy* strategy() const { return stopStrategy_; }

    // Actions for states
    void notifyArrive() { if (observer_) observer_->onArrive(currentFloor_); }
    void openDoor() { setDoorState(doorOpenState_); }
    void closeDoor() { setDoorState(doorClosedState_); }
    void moveUpState() { setMoveState(movingUpState_); }
    void moveDownState() { setMoveState(movingDownState_); }
    void moveIdleState() { setMoveState(idleState_); }

    void removeUpStop(Floor f) { popMatch(upQueue_, f); }
    void removeDownStop(Floor f) { popMatch(downQueue_, f); }

    void incrementFloorUp() { if (currentFloor_ < maxFloor_) ++currentFloor_; }
    void decrementFloorDown() { if (currentFloor_ > minFloor_) --currentFloor_; }

    // Decision hook for states
    void decideNextDirectionIfIdle() {
        if (moveState_ != idleState_) return;
        int dir = stopStrategy_ ? stopStrategy_->chooseNextDirection(currentFloor_, upQueue_, downQueue_)
                                : Direction::Idle();
        if (dir == Direction::Up()) moveUpState();
        else if (dir == Direction::Down()) moveDownState();
        else moveIdleState();
    }

private:
    // Internal helpers
    void initStates();

    void setDoorState(DoorState* s) {
        doorState_ = s;
        doorState_->onEnter(*this);
        if (observer_) observer_->onDoorChange(doorState_ == doorOpenState_ ? 1 : 0);
    }
    void setMoveState(MovementState* s) {
        moveState_ = s;
        moveState_->onEnter(*this);
    }

    bool inRange(Floor f) const { return f >= minFloor_ && f <= maxFloor_; }

    static bool contains(const std::vector<Floor>& v, Floor f) {
        for (size_t i=0;i<v.size();++i) if (v[i]==f) return true; return false;
    }
    static void insertSortedAsc(std::vector<Floor>& v, Floor f) {
        size_t i=0; while (i<v.size() && v[i] < f) ++i; v.insert(v.begin()+i, f);
    }
    static void insertSortedDesc(std::vector<Floor>& v, Floor f) {
        size_t i=0; while (i<v.size() && v[i] > f) ++i; v.insert(v.begin()+i, f);
    }
    static void popMatch(std::vector<Floor>& v, Floor f) {
        for (size_t i=0;i<v.size();++i) if (v[i]==f) { v.erase(v.begin()+i); return; }
    }

    void enqueueStop(Floor dest) {
        if (dest == currentFloor_) {
            openDoor();
            return;
        }
        if (dest > currentFloor_) {
            if (!contains(upQueue_, dest)) insertSortedAsc(upQueue_, dest);
        } else {
            if (!contains(downQueue_, dest)) insertSortedDesc(downQueue_, dest);
        }
        decideNextDirectionIfIdle();
    }

private:
    // Data
    Floor currentFloor_;
    int minFloor_, maxFloor_;
    int capacity_;
    int passengers_;

    std::vector<Floor> upQueue_;
    std::vector<Floor> downQueue_;

    StopSelectionStrategy* stopStrategy_;
    ElevatorObserver* observer_;

    // States (owned)
    DoorState* doorOpenState_;
    DoorState* doorClosedState_;
    MovementState* movingUpState_;
    MovementState* movingDownState_;
    MovementState* idleState_;

    // Current states
    DoorState* doorState_;
    MovementState* moveState_;
};

// ===================== Concrete Door States =====================
class DoorOpen : public DoorState {
public:
    int dwellTicks;
    DoorOpen(): dwellTicks(0) {}
    void onEnter(Elevator& e) override {
        dwellTicks = 1; // simple dwell
        // onDoorChange is called by Elevator::setDoorState
    }
    void onTick(Elevator& e) override {
        if (dwellTicks > 0) {
            dwellTicks--;
            if (dwellTicks == 0) e.closeDoor();
        }
    }
    const char* name() const override { return "DoorOpen"; }
};

class DoorClosed : public DoorState {
public:
    void onEnter(Elevator& e) override { (void)e; }
    void onTick(Elevator& e) override { (void)e; }
    const char* name() const override { return "DoorClosed"; }
};

// ===================== Concrete Movement States =====================
class MovingUp : public MovementState {
public:
    void onEnter(Elevator& e) override { (void)e; }
    void onTick(Elevator& e) override {
        e.incrementFloorUp();
        if (e.observer()) e.observer()->onArrive(e.currentFloor());
        if (e.atUpStop()) {
            e.removeUpStop(e.currentFloor());
            e.openDoor();
            if (!e.hasUpStops() && !e.hasDownStops()) e.moveIdleState();
            else if (!e.hasUpStops() && e.hasDownStops()) e.moveDownState();
        }
    }
    const char* name() const override { return "MovingUp"; }
};

class MovingDown : public MovementState {
public:
    void onEnter(Elevator& e) override { (void)e; }
    void onTick(Elevator& e) override {
        e.decrementFloorDown();
        if (e.observer()) e.observer()->onArrive(e.currentFloor());
        if (e.atDownStop()) {
            e.removeDownStop(e.currentFloor());
            e.openDoor();
            if (!e.hasDownStops() && !e.hasUpStops()) e.moveIdleState();
            else if (!e.hasDownStops() && e.hasUpStops()) e.moveUpState();
        }
    }
    const char* name() const override { return "MovingDown"; }
};

class Idle : public MovementState {
public:
    void onEnter(Elevator& e) override {
        int dir = e.strategy() ? e.strategy()->chooseNextDirection(e.currentFloor(), e.hasUpStops()?std::vector<Floor>{e.currentFloor()} : std::vector<Floor>(),
                                                                   e.hasDownStops()?std::vector<Floor>{e.currentFloor()} : std::vector<Floor>())
                               : Direction::Idle();
        (void)dir; // We’ll rely on Elevator::decideNextDirectionIfIdle elsewhere
    }
    void onTick(Elevator& e) override {
        // Nothing; direction decision is triggered via enqueueStop() or after doors close
        (void)e;
    }
    const char* name() const override { return "Idle"; }
};

// State creation
void Elevator::initStates() {
    doorOpenState_ = new DoorOpen();
    doorClosedState_ = new DoorClosed();
    movingUpState_ = new MovingUp();
    movingDownState_ = new MovingDown();
    idleState_ = new Idle();
}

// ===================== Demo Main =====================
int main() {
    ConsoleObserver observer;
    NearestStopStrategy strategy;

    Elevator lift(0, 0, 20, 10, &strategy, &observer);

    // Scenario
    lift.requestHall(HallCall(7, Direction::Up()));
    lift.requestCar(CarCall(15));
    lift.requestHall(HallCall(3, Direction::Down()));

    for (int t = 0; t < 50; ++t) {
        lift.step();
        if (lift.isIdle()) break;
    }
    return 0;
}