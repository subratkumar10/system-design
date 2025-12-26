
#include<iostream>
#include<thread>
#include<mutex>
#include<chrono>
using namespace std;

int balance =0;
condition_variable cv;
mutex m1;
void addMoney(int money){

    lock_guard<mutex> lock(m1);
    cout<<"adding money : "<<money<<endl;
    balance+=money;
     cv.notify_one();

}




void withdraw(int money){

    unique_lock<mutex> lock(m1);
    cv.wait(lock,[]{ return balance>0; });

    if(balance>=money){
    balance-=money;
    } else {
        cout<<"Not available balance for withdrawl";
        return;
    }

}


int main() {


 
    thread t2(withdraw,40);
     this_thread::sleep_for(chrono::seconds(2));
        thread t1(addMoney,0);

    t1.join();
    t2.join();

    cout<<"Available balance: "<<balance<<endl;


}