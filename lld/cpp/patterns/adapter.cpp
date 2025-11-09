
#include<iostream>
#include <memory>
using namespace std;


class RoundPlug {
public:
    virtual void insertIntoSocket() = 0;
    virtual ~RoundPlug() = default;
};

// Adaptee (Existing / Legacy class with a different interface)
class SquarePlug {
public:
    void insertIntoSquareSocket() {
        cout << "Square plug inserted into square socket." << endl;
    }
};

// Adapter (Makes SquarePlug compatible with RoundPlug)
class SquareToRoundAdapter : public RoundPlug {
private:
    SquarePlug* squarePlug;  // Composition

public:
    SquareToRoundAdapter(SquarePlug* plug) : squarePlug(plug) {}

    void insertIntoSocket() override {
        cout << "Adapter converts round plug to square plug interface..." << endl;
        squarePlug->insertIntoSquareSocket();
    }
};

int main() {


    SquarePlug* squarePlug = new SquarePlug();

    RoundPlug* roundPlug = new SquareToRoundAdapter(squarePlug);

    roundPlug-> insertIntoSocket();

    delete roundPlug;
}