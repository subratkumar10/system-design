


#include<iostream>
#include<thread>
#include<mutex>
#include<chrono>
using namespace std;

// deadlock situation occurs when cyclic dependencies is encountered

mutex m1,m2;


void fun1(){

    m1.lock();
    m2.lock();

    cout<<"Executing critical section in thread 1"<<endl;
    m2.unlock();
    m1.unlock();

}

void fun2(){

    m2.lock();
    m1.lock();


    cout<<"Executing critical section in thread 2"<<endl;
 
    m1.unlock();

    m2.unlock();
}


int main() {


 

    thread t1(fun1);
    thread t2(fun2);
     // this_thread::sleep_for(chrono::seconds(2));
  
    t1.join();
    t2.join();




}