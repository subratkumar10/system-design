#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
using namespace std;

// Observer Interface
class Observer {
public:
    virtual void update(int state) = 0;
    virtual ~Observer() = default;
};

// Subject Interface
class Subject {
public:
    virtual void attach(shared_ptr<Observer> obs) = 0;
    virtual void detach(shared_ptr<Observer> obs) = 0;
    virtual void notify() = 0;
    virtual ~Subject() = default;
};

// Concrete Subject
class ConcreteSubject : public Subject {
private:
    vector<shared_ptr<Observer>> observers;
    int state;
public:
    void attach(shared_ptr<Observer> obs) override {
        observers.push_back(obs);
    }

    void detach(shared_ptr<Observer> obs) override {
        observers.erase(
            remove(observers.begin(), observers.end(), obs), 
            observers.end()
        );
    }

    void notify() override {
        for (auto &obs : observers)
            obs->update(state);
    }

    void setState(int s) {
        state = s;
        notify();  // automatically notify observers
    }

    int getState() const { return state; }
};

// Concrete Observer
class ConcreteObserver : public Observer {
private:
    string name;
public:
    ConcreteObserver(const string &n) : name(n) {}
    void update(int state) override {
        cout << "Observer " << name << " received update: state = " << state << endl;
    }
};

int main() {
    auto subject = make_shared<ConcreteSubject>();

    auto obs1 = make_shared<ConcreteObserver>("A");
    auto obs2 = make_shared<ConcreteObserver>("B");

    subject->attach(obs1);
    subject->attach(obs2);

    subject->setState(10);
    subject->setState(20);

    subject->detach(obs1);
    subject->setState(30);

    return 0;
}