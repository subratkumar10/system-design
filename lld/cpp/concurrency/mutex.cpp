#include<iostream>
#include<thread>
#include<chrono>
#include<mutex>
using namespace std;


mutex m;


int currScore =0;

void increment(int n){
    // lock_guard<mutex> lg(m);
    m.lock();
    for(int i=0;i<n;i++)
     currScore++;
     m.unlock();
}

int main() {


    thread t1(increment,10);
    thread t2(increment,10);

    t1.join();
    t2.join();
    cout<<"currScore : "<<currScore<<endl;
}

