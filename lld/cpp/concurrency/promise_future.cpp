


#include<iostream>
#include<thread>
#include<mutex>
#include<chrono>
#include<future>
using namespace std;


// void calc_oddsum(int start,int end,promise<int>&&oddSum){

//     int count=0;
//     for(int i=start;i<=end;i++){
//         if(i&1)
//         count++;
//     }
//     oddSum.set_value(count);
// }

void calc_oddsum(int start,int end,int* oddSum){

    int count=0;
    for(int i=start;i<=end;i++){
        if(i&1)
        count++;
    }
    (*oddSum) = count;
}
int main() {

    int start =0;
    int end = 20;
    // promise<int> oddSum;
    // future<int> futureOddsum = oddSum.get_future();
    int oddSum =0 ;

    // thread t1(calc_oddsum,start,end,move(oddSum));
    thread t1(calc_oddsum,start,end,&oddSum);
 
    cout<<"waiting for calculation: ";

    cout<<"oddsum: "<<oddSum<<endl;
  
     // this_thread::sleep_for(chrono::seconds(2));
  
    t1.join();





}