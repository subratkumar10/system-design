#include <iostream>
#include <memory>
#include <string>

class Vehicle {
public:
    virtual void info() = 0; // pure virtual function
    virtual ~Vehicle() = default;
};

class Car : public Vehicle {
public:
    void info() override {
        std::cout << "This is a Car" << std::endl;
    }
};

class Bike : public Vehicle {
public:
    void info() override {
        std::cout << "This is a Bike" << std::endl;
    }
};

class VehicleFactory {
public:
    static std::unique_ptr<Vehicle> createVehicle(const std::string& type) {
        if (type == "Car") {
            return std::make_unique<Car>();
        } else if (type == "Bike") {
            return std::make_unique<Bike>();
        } else {
            return nullptr;
        }
    }
};

int main() {
    auto car = VehicleFactory::createVehicle("Car");
    auto bike = VehicleFactory::createVehicle("Bike");

    if(car) car->info();  // Output: This is a Car
    if(bike) bike->info(); // Output: This is a Bike

    return 0;
}