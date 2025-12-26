#include <iostream>
#include<mutex>
#include<condition_variable>
#include <thread>
#include<map>
#include<unordered_map>

using namespace std;


enum class Status {
PENDING,
INPROGRESS,
RESOLVED,
CANCELLED
};

enum class Priority {
    LOW = 0,
    MEDIUM = 1 ,
    HIGH = 2
};

class Task {
    public: 
    int id;
    string title;
    Status status;
    Priority priority;
    Task(int id, string title, Status status,Priority priority): id(id), title(title), status(status), priority(priority){}

};

class SchedulerPolicy {

    public:
    virtual int  pickNextTask(unordered_map<int,Task*> map) = 0;
    virtual ~SchedulerPolicy(){};
};


class HighestPriority : public SchedulerPolicy {


public:
    HighestPriority(){}
   int  pickNextTask(unordered_map<int,Task*> mp) {

        int id = -1;
        int maxVal =-1;
        for(auto &x:mp) {

            if(x.second->status == Status::PENDING) {
                cout<<x.second->title<<endl;
                if(maxVal < (int)(x.second-> priority)) {
                    maxVal = (int)(x.second-> priority);
                    id = x.second->id;
                }
            }
        }
        cout<<id<<endl;
        return id;

    }


};


string mapStatusToString(Status status){

    switch (status) {
        case Status::CANCELLED: return "CANCELLED";
        case Status::INPROGRESS: return "INPROGRESS";
        case Status::PENDING: return "PENDING";
        case Status::RESOLVED: return "RESOLVED";
    }
    return "";
}




class TaskManager {

    int taskQueueSize;
    unordered_map<int,Task*> taskMap;
    mutex mu;
    atomic<int> nextId;
    condition_variable cv;

    static TaskManager* taskManager;

    TaskManager(int taskQueueSize) : taskQueueSize(taskQueueSize){}

    public:

    static TaskManager* getInstance(int taskQueueSize){

        if(taskManager==nullptr){
            taskManager = new TaskManager(taskQueueSize);
        }
        return taskManager;
    }

    

    bool addTask(string title,
                  Priority priority
                ) {

     unique_lock<mutex> lock(mu);
     bool success = cv.wait_for(lock,std::chrono::seconds(3),[&]{
        return canTaskBeAdded();
     });
     if(!success)
     {
        throw runtime_error("size is insufficient");
     }

     int id = nextId.fetch_add(1);
    Task* task = new Task(id,title,Status::PENDING,priority);
    cout<<"id "<<id<<endl;
    taskMap[id] = task;
    cout<<"Task : "<<title<<" is Added"<<endl;
    return true;
    
    }

    bool updateTask(int id, Status status) {
        lock_guard<mutex>lock(mu);
        auto pos = taskMap.find(id);
        if(pos== taskMap.end())
        return false;
        pos->second->status = status;
        cout<<"changed status of id "<<id<<" ->"<<mapStatusToString(status)<<endl;
        cv.notify_all();
        return true;
        
    }

    bool executeNextTask(SchedulerPolicy* schedulerPolicy){

        unique_lock<mutex>lock(mu);
        int ok = cv.wait_for(lock,chrono::seconds(10),[&] { return canNextTaskbeExecuted();});
        if(!ok)
        {
            throw runtime_error("timeout error, no task can be executed next");
        }
        int nextId = schedulerPolicy->pickNextTask(taskMap);
        if(nextId == -1){
            cout<<"No task to be executed"<<endl;
        return false;
        }

        taskMap[nextId]-> status = Status::INPROGRESS;
                cout<<"Next task added details : id "<< taskMap[nextId]->id << "title "<<taskMap[nextId]->title<<" prioritity:  "<< (int)taskMap[nextId]->priority<<"  status : "<< mapStatusToString(taskMap[nextId]->status)<<endl;
        return true;

    }

    bool canNextTaskbeExecuted() {

         int count=0;
        for(auto &x: taskMap){
            if(x.second->status == Status::PENDING)
            count++;
        }
        return count>0;

    }

    bool canTaskBeAdded() {
        int count=0;
        for(auto &x: taskMap){
            if((x.second->status == Status::PENDING) || (x.second->status == Status::INPROGRESS))
            count++;
        }
        return count<taskQueueSize;
    }


};

TaskManager* TaskManager::taskManager = nullptr;

int main(){


    TaskManager* taskManager = TaskManager::getInstance(3);

    thread producer([&]{
    taskManager->addTask("task1",Priority::LOW);
    taskManager->addTask("task2",Priority::MEDIUM);
     taskManager->addTask("task3",Priority::LOW);
    });
       SchedulerPolicy* schedulerPolicy = new HighestPriority();

       this_thread::sleep_for(chrono::seconds(3));
    thread consumer1([&]{
     
        taskManager->executeNextTask(schedulerPolicy);
    });

    thread consumer2([&]{
     
        taskManager->executeNextTask(schedulerPolicy);
    });

     thread consumer3([&]{
     
        taskManager->executeNextTask(schedulerPolicy);
    });
      thread consumer4([&]{
     
        taskManager->executeNextTask(schedulerPolicy);
    });

    this_thread::sleep_for(chrono::seconds(3));
    
      thread t1([&]{
     
        taskManager->updateTask(1,Status::PENDING);
    });

    producer.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();
    t1.join();
    // cout<<"hello "<<endl;




    return 0;
}









