#include<iostream>
#include<string>
using namespace std;
// Builder pattern


class Car {
    private:
    string number;
    string engine;
    int seat;

    public:

    void setNumber(string number) {
        this-> number = number;
    }
    void setEngine(string engine) {
        this->engine = engine;
    }
    void setSeat(int val) {
        this-> seat = val;
    }

      void show() const {
        cout << "Car number " << number
             << ", Engine: " << engine 
             << ", Seats: " << seat<< "]\n";
    }

};

class CarBuilder {

    private:
    Car car;

    public:
    CarBuilder& setNumber(string number) {
        car.setNumber(number);
        return *this;
    }
    CarBuilder& setEngine(string engine) {
         car.setEngine(engine);
        return *this;
    }
     CarBuilder& setSeat(int val) {
         car.setSeat(val);
        return *this;
    }
    Car build() {
        return this->car;  // Return the final object
    }
};


int main() {

    Car car = CarBuilder().setNumber("123OD").setSeat(6).setEngine("120c").build();
    car.show();

}