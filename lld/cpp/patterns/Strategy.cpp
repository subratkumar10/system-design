
//strategy pattern in c++

#include <iostream>
#include <memory>
#include <string>
using namespace std;

// Step 1: Define the Strategy Interface
class PaymentStrategy {
public:
    virtual void pay(double amount) const = 0;
    virtual ~PaymentStrategy() = default;
};

// Step 2: Implement Concrete Strategies

class CreditCardPayment : public PaymentStrategy {
    string cardNumber;
public:
    CreditCardPayment(const string& card) : cardNumber(card) {}
    void pay(double amount) const override {
        cout << "Paid ₹" << amount << " using Credit Card [" << cardNumber << "] 💳" << endl;
    }
};

class PayPalPayment : public PaymentStrategy {
    string email;
public:
    PayPalPayment(const string& emailAddr) : email(emailAddr) {}
    void pay(double amount) const  override {
        cout << "Paid ₹" << amount << " using PayPal account [" << email << "] 💰" << endl;
    }
};

class CryptoPayment : public PaymentStrategy {
    string walletId;
public:
    CryptoPayment(const string& wallet) : walletId(wallet) {}
    void pay(double amount) const override  {
        cout << "Paid ₹" << amount << " using Crypto wallet [" << walletId << "] ₿" << endl;
    }
};

enum PaymentMode {
    CARD,
    CRYPTO,
    PAYPAL
};

// Step 3: Context class — uses a Strategy
class PaymentContext {
private:
    unique_ptr<PaymentStrategy> strategy;
public:
    void setStrategy(PaymentMode paymentMode,string id) {

        if(paymentMode == CARD) {
            strategy = unique_ptr<CreditCardPayment>(new CreditCardPayment(id));
        } else if (paymentMode == CRYPTO) {
            strategy = unique_ptr<CryptoPayment>(new CryptoPayment(id));
         } else if (paymentMode == PAYPAL) {
            strategy = unique_ptr<PayPalPayment>(new PayPalPayment(id));
         }
        
    }

    void checkout(double amount) const {
        if (strategy)
            strategy->pay(amount);
        else
            cout << "Error: No payment method selected!" << endl;
    }
};

// Step 4: Client code
int main() {
    PaymentContext context;

    // User selects Credit Card
    context.setStrategy(PaymentMode::CARD,"1234-5678-9876");
    context.checkout(1200.50);

    // Switch to PayPal
    context.setStrategy(PaymentMode::PAYPAL,"user@example.com");
    context.checkout(850.00);

    // Switch to Crypto
    context.setStrategy(PaymentMode::CRYPTO,"wallet_abc123");
    context.checkout(5000.00);

    return 0;
}

