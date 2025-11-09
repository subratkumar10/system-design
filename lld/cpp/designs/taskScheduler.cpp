#include <iostream>
#include <queue>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

// ------------------- Task (Command Pattern) -------------------
class Task {
protected:
    string name;
    int priority;
public:
    Task(string n, int p) : name(n), priority(p) {}
    virtual void execute() = 0; // Task logic
    virtual ~Task() = default;
    int getPriority() const { return priority; }
    string getName() const { return name; }
};

// Example Task
class PrintTask : public Task {
    string message;
public:
    PrintTask(string msg, int p) : Task("PrintTask", p), message(msg) {}
    void execute() override {
        cout << "Executing task: " << message << endl;
    }
};

// ------------------- Task Factory -------------------
class TaskFactory {
public:
    static shared_ptr<Task> createTask(const string& type, const string& msg, int priority) {
        if(type == "print") return make_shared<PrintTask>(msg, priority);
        // Can add more task types
        return nullptr;
    }
};

// ------------------- Observer -------------------
class TaskListener {
public:
    virtual void onTaskComplete(shared_ptr<Task> task) = 0;
    virtual ~TaskListener() = default;
};

// ------------------- Task Queue (Strategy / Priority Queue) -------------------
struct TaskComparator {
    bool operator()(shared_ptr<Task> a, shared_ptr<Task> b) {
        return a->getPriority() < b->getPriority(); // Higher priority first
    }
};

// ------------------- Task Scheduler (Singleton + ThreadPool) -------------------
class TaskScheduler {
private:
    priority_queue<shared_ptr<Task>, vector<shared_ptr<Task>>, TaskComparator> taskQueue;
    vector<shared_ptr<TaskListener>> listeners;
    mutex mtx;
    condition_variable cv;
    bool stop = false;
    vector<thread> workers;

    // Private constructor for Singleton
    TaskScheduler(int numThreads = 2) {
        for(int i = 0; i < numThreads; i++)
            workers.emplace_back([this]() { this->workerThread(); });
    }

    void workerThread() {
        while(true) {
            shared_ptr<Task> task;
            {
                unique_lock<mutex> lock(mtx);
                cv.wait(lock, [this]() { return !taskQueue.empty() || stop; });
                if(stop && taskQueue.empty()) return;
                task = taskQueue.top();
                taskQueue.pop();
            }
            task->execute();
            notifyListeners(task);
        }
    }

    void notifyListeners(shared_ptr<Task> task) {
        for(auto& listener : listeners)
            listener->onTaskComplete(task);
    }

public:
    static shared_ptr<TaskScheduler> getInstance(int numThreads = 2) {
        static shared_ptr<TaskScheduler> instance(new TaskScheduler(numThreads));
        return instance;
    }

    void addTask(shared_ptr<Task> task) {
        {
            lock_guard<mutex> lock(mtx);
            taskQueue.push(task);
        }
        cv.notify_one();
    }

    void attachListener(shared_ptr<TaskListener> listener) {
        listeners.push_back(listener);
    }

    void shutdown() {
        {
            lock_guard<mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for(auto& t : workers) t.join();
    }
};

// ------------------- Demo Listener -------------------
class LoggerListener : public TaskListener {
public:
    void onTaskComplete(shared_ptr<Task> task) override {
        cout << "Task completed: " << task->getName() << endl;
    }
};

// ------------------- Demo -------------------
int main() {
    auto scheduler = TaskScheduler::getInstance(3);
    scheduler->attachListener(make_shared<LoggerListener>());

    scheduler->addTask(TaskFactory::createTask("print", "Hello World!", 1));
    scheduler->addTask(TaskFactory::createTask("print", "High Priority Task", 10));
    scheduler->addTask(TaskFactory::createTask("print", "Low Priority Task", 0));

    this_thread::sleep_for(chrono::seconds(2));
    scheduler->shutdown();
    return 0;
}
