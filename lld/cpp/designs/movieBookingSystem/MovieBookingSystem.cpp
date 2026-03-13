#include <iostream>
#include <vector>
#include <string>

// User base class

/* patterns used 
Singleton pattern:  BookingSystem class
Strategy pattern:  Payment method


*/
class User {
protected:
    std::string username;
public:
    User(const std::string& name) : username(name) {}
    virtual ~User() {}
    std::string getUsername() const { return username; }
};

class Customer : public User {
public:
    Customer(const std::string& name) : User(name) {}
};

class Admin : public User {
public:
    Admin(const std::string& name) : User(name) {}
};

// Movie class
class Movie {
    std::string title;
public:
    Movie(const std::string& t) : title(t) {}
    std::string getTitle() const { return title; }
};

// Seat class
class Seat {
    int row, col;
    bool booked;
public:
    Seat(int r, int c) : row(r), col(c), booked(false) {}
    bool isBooked() const { return booked; }
    void book() { booked = true; }
    int getRow() const { return row; }
    int getCol() const { return col; }
};

// Show class
class Show {
    Movie movie;
    std::string showTime;
    std::vector<Seat> seats;
public:
    Show(const Movie& m, const std::string& t, int seatCount)
            : movie(m), showTime(t) {
        for(int i = 0; i < seatCount; ++i)
            seats.emplace_back(i/10, i%10);
    }
    Movie getMovie() const { return movie; }
    std::string getShowTime() const { return showTime; }
    std::vector<Seat>& getSeats() { return seats; }
};

// Theatre class
class Theatre {
    std::string name;
    std::vector<Show> shows;
public:
    Theatre(const std::string& n) : name(n) {}
    void addShow(const Show& show) { shows.push_back(show); }
    std::vector<Show>& getShows() { return shows; }
    std::string getName() const { return name; }
};

// Booking class
class Booking {
    Customer* customer;
    Show* show;
    Seat* seat;
public:
    Booking(Customer* c, Show* s, Seat* st) : customer(c), show(s), seat(st) {}
    void printBooking() {
        std::cout << "Booking for " << customer->getUsername() << " - Movie: "
                  << show->getMovie().getTitle() << ", Time: "
                  << show->getShowTime() << ", Seat: ["
                  << seat->getRow() << "," << seat->getCol()
                  << "]\n";
    }
};

// Payment base & derived (Factory pattern)
class Payment {
public:
    virtual bool pay(double amount) = 0;
    virtual ~Payment() {}
};

class CreditCardPayment : public Payment {
public:
    bool pay(double amount) override {
        std::cout << "Paid $" << amount << " using Credit Card.\n";
        return true;
    }
};

class UpiPayment : public Payment {
public:
    bool pay(double amount) override {
        std::cout << "Paid $" << amount << " using UPI.\n";
        return true;
    }
};

class PaymentFactory {
public:
    static Payment* createPayment(const std::string& type) {
        if (type == "credit_card")
            return new CreditCardPayment();
        else if (type == "upi")
            return new UpiPayment();
        else
            return nullptr;
    }
};

// Singleton BookingSystem
class BookingSystem {
    static BookingSystem* instance;
    std::vector<Theatre> theatres;
    BookingSystem() {}
public:
    static BookingSystem* getInstance() {
        if (!instance) instance = new BookingSystem();
        return instance;
    }
    void addTheatre(const Theatre& t) { theatres.push_back(t); }
    std::vector<Theatre>& getTheatres() { return theatres; }
    // Note: Add destructor and cleanup in production systems
};
BookingSystem* BookingSystem::instance = nullptr;


int main() {
    BookingSystem* system = BookingSystem::getInstance();

    // Setup
    Theatre myTheatre("Oracle Cinemas");
    Movie avatar("Avatar 2");
    Show eveningShow(avatar, "18:00", 20);
    myTheatre.addShow(eveningShow);
    system->addTheatre(myTheatre);

    // Booking flow
    Customer alice("Alice");
    Theatre& t = system->getTheatres()[0];
    Show& s = t.getShows()[0];

    // Find a free seat
    Seat* foundSeat = nullptr;
    for (auto& seat : s.getSeats()) {
        if (!seat.isBooked()) {
           foundSeat = &seat;
            seat.book();
            break;
        }
    }

    if (foundSeat) {
        Booking booking(&alice, &s, foundSeat);
        booking.printBooking();
        // Payment
        Payment* payMethod = PaymentFactory::createPayment("credit_card");
        if (payMethod) {
            payMethod->pay(200.0);
            delete payMethod;
        }
    } else {
        std::cout << "No seat available!\n";
    }

    return 0;
}
