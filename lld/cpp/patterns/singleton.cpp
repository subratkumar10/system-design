//singleton pattern normal


#include<iostream>
#include<mutex>
#include <memory> 
using namespace std;

class Singleton {

    private:

    Singleton(){}
    static unique_ptr<Singleton> instance;

    public:

    static Singleton* getInstance(){

        if(instance == NULL) {
            
            instance = unique_ptr<Singleton>(new Singleton());
        }
        return instance.get();
    }

};

unique_ptr<Singleton> Singleton::instance = nullptr;


int main() {

    Singleton* obj1 = Singleton::getInstance();
    Singleton* obj2 = Singleton::getInstance();

    cout<<(obj1 == obj2)<<endl;
    
}