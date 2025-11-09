//singleton pattern with thread safe 


#include<iostream>
#include<mutex>
#include <memory> 
#include <thread>
#include<vector>
using namespace std;

class Singleton {

    private:

    Singleton(){}
    static unique_ptr<Singleton> instance;
    static mutex mtx;

    public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static Singleton* getInstance(){

        lock_guard<mutex> lock(mtx);

        if(instance == NULL) {
            
            instance = unique_ptr<Singleton>(new Singleton());
        }
        return instance.get();
    }

};

unique_ptr<Singleton> Singleton::instance = nullptr;

mutex Singleton::mtx;
mutex printMtx;
void threadFunc(int id) {
    Singleton* obj = Singleton::getInstance();
    lock_guard<mutex> lock(printMtx);
    cout << "Thread " << id << " got instance at " << obj << endl;
}

int main() {

      const int numThreads = 5;
    vector<thread> threads;

    // Launch multiple threads
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(threadFunc, i);
    }

    // Wait for all threads
    for (auto &t : threads) {
        t.join();
    }
    
}