#include <iostream>
#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <thread>
#include <memory>
#include <functional>

// ------------------- Domain -------------------
enum class Status { Pending, InProgress, Completed, Cancelled };
enum class Priority { Low = 0, Medium = 1, High = 2, Critical = 3 };

struct Task {
    int id = 0;
    std::string title;
    std::string description;
    std::chrono::system_clock::time_point due;
    Status status = Status::Pending;
    std::string assignee;
    Priority priority = Priority::Medium;
};

// ------------------- Factory Pattern -------------------
class TaskFactory {
public:
    TaskFactory() : nextId_(1) {}

    Task create(const std::string& title,
                const std::string& desc,
                const std::chrono::system_clock::time_point& due,
                const std::string& assignee,
                Priority prio) {
        Task t;
        t.id = nextId_.fetch_add(1, std::memory_order_relaxed);
        t.title = title;
        t.description = desc;
        t.due = due;
        t.status = Status::Pending;
        t.assignee = assignee;
        t.priority = prio;
        return t;
    }

private:
    std::atomic<int> nextId_;
};

// ------------------- Observer Pattern -------------------
struct ITaskListener {
    virtual ~ITaskListener() {}
    virtual void onTaskAdded(const Task& t) {}
    virtual void onTaskUpdated(const Task& t) {}
    virtual void onStatusChanged(const Task& t, Status oldStatus, Status newStatus) {}
};

// ------------------- Strategy Pattern (Scheduling) -------------------
struct ISchedulingPolicy {
    virtual ~ISchedulingPolicy() {}
    // Return index of chosen task in the provided vector snapshot; -1 if none
    virtual int selectNextPending(const std::vector<Task>& snapshot) const = 0;
};

// Priority-aware strategy: higher priority first, then earliest due, then lowest id
class PrioritySchedulingPolicy : public ISchedulingPolicy {
public:
    int selectNextPending(const std::vector<Task>& v) const override {
        int best = -1;
        for (int i = 0; i < static_cast<int>(v.size()); ++i) {
            if (v[i].status != Status::Pending) continue;
            if (best == -1 || better(v[i], v[best])) best = i;
        }
        return best;
    }

private:
    static bool better(const Task& a, const Task& b) {
        if (static_cast<int>(a.priority) != static_cast<int>(b.priority))
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        if (a.due != b.due) return a.due < b.due;
        return a.id < b.id;
    }
};

// FIFO strategy by id (older id first) among Pending
class FifoSchedulingPolicy : public ISchedulingPolicy {
public:
    int selectNextPending(const std::vector<Task>& v) const override {
        int best = -1;
        for (int i = 0; i < static_cast<int>(v.size()); ++i) {
            if (v[i].status != Status::Pending) continue;
            if (best == -1 || v[i].id < v[best].id) best = i;
        }
        return best;
    }
};

// ------------------- Task Manager (Thread-safe) -------------------
class TaskManager {
public:
    explicit TaskManager(std::unique_ptr<ISchedulingPolicy> policy)
        : policy_(std::move(policy)) {}

    // Observer registration
    void addListener(std::shared_ptr<ITaskListener> l) {
        std::lock_guard<std::mutex> lock(mtx_);
        listeners_.push_back(std::move(l));
    }

    // Create
    int addTask(const std::string& title,
                const std::string& description,
                const std::chrono::system_clock::time_point& due,
                const std::string& assignee,
                Priority priority) {
        Task t = factory_.create(title, description, due, assignee, priority);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_[t.id] = t;
            notifyAddedUnsafe(t);
        }
        cv_.notify_one();
        return t.id;
    }

    // Update fields
    bool updateTaskFields(int id,
                          const std::string& title,
                          const std::string& description,
                          const std::chrono::system_clock::time_point& due,
                          const std::string& assignee,
                          Priority priority) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        it->second.title = title;
        it->second.description = description;
        it->second.due = due;
        it->second.assignee = assignee;
        it->second.priority = priority;
        notifyUpdatedUnsafe(it->second);
        if (it->second.status == Status::Pending) cv_.notify_one();
        return true;
    }

    // Update status
    bool updateStatus(int id, Status newStatus) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        Status old = it->second.status;
        it->second.status = newStatus;
        notifyStatusUnsafe(it->second, old, newStatus);
        if (newStatus == Status::Pending) cv_.notify_one();
        return true;
    }

    // Delete
    bool deleteTask(int id) {
        std::lock_guard<std::mutex> lock(mtx_);
        return tasks_.erase(id) > 0;
    }

    // Queries (return snapshots/copies)
    bool getTask(int id, Task& out) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<Task> listAll() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<Task> v;
        v.reserve(tasks_.size());
        for (const auto& kv : tasks_) v.push_back(kv.second);
        return v;
    }

    std::vector<Task> listSortedByPriority() const {
        auto v = listAll();
        std::sort(v.begin(), v.end(), [](const Task& a, const Task& b) {
            if (static_cast<int>(a.priority) != static_cast<int>(b.priority))
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            if (a.due != b.due) return a.due < b.due;
            return a.id < b.id;
        });
        return v;
    }

    std::vector<Task> listByAssignee(const std::string& who) const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<Task> v;
        for (const auto& kv : tasks_) if (kv.second.assignee == who) v.push_back(kv.second);
        return v;
    }

    // Worker APIs
    Task takeNextTask() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]{ return hasPendingUnsafe(); });

        // Snapshot to use strategy safely
        std::vector<Task> snap;
        snap.reserve(tasks_.size());
        for (const auto& kv : tasks_) snap.push_back(kv.second);

        int idx = policy_->selectNextPending(snap);
        if (idx < 0) {
            lock.unlock();
            return takeNextTask(); // spurious wakeup safeguard
        }

        int chosenId = snap[idx].id;
        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) {
            lock.unlock();
            return takeNextTask(); // contention: retry
        }

        Status old = it->second.status;
        it->second.status = Status::InProgress;
        Task result = it->second;
        notifyStatusUnsafe(result, old, Status::InProgress);
        return result;
    }

    bool tryTakeNextTask(Task& out) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!hasPendingUnsafe()) return false;

        std::vector<Task> snap;
        snap.reserve(tasks_.size());
        for (const auto& kv : tasks_) snap.push_back(kv.second);
        int idx = policy_->selectNextPending(snap);
        if (idx < 0) return false;

        int chosenId = snap[idx].id;
        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) return false;

        Status old = it->second.status;
        it->second.status = Status::InProgress;
        out = it->second;
        notifyStatusUnsafe(out, old, Status::InProgress);
        return true;
    }

    void notifyAll() { cv_.notify_all(); }

private:
    // Observer dispatch (mtx_ must be held)
    void notifyAddedUnsafe(const Task& t) {
        for (auto& w : listeners_) if (w) w->onTaskAdded(t);
    }
    void notifyUpdatedUnsafe(const Task& t) {
        for (auto& w : listeners_) if (w) w->onTaskUpdated(t);
    }
    void notifyStatusUnsafe(const Task& t, Status oldS, Status newS) {
        for (auto& w : listeners_) if (w) w->onStatusChanged(t, oldS, newS);
    }

    bool hasPendingUnsafe() const {
        for (const auto& kv : tasks_) if (kv.second.status == Status::Pending) return true;
        return false;
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::map<int, Task> tasks_;
    TaskFactory factory_;
    std::unique_ptr<ISchedulingPolicy> policy_;
    std::vector<std::shared_ptr<ITaskListener>> listeners_;
};

// ------------------- Example listener -------------------
class LoggingListener : public ITaskListener {
public:
    void onTaskAdded(const Task& t) override {
        std::cout << "[listener] Added task #" << t.id << " (" << t.title << ")\n";
    }
    void onStatusChanged(const Task& t, Status oldS, Status newS) override {
        std::cout << "[listener] Status #" << t.id << " " << static_cast<int>(oldS)
                  << " -> " << static_cast<int>(newS) << "\n";
    }
};

// ------------------- Demo main -------------------
int main() {
    // Choose scheduling policy
    // auto policy = std::unique_ptr<ISchedulingPolicy>(new FifoSchedulingPolicy());
    auto policy = std::unique_ptr<ISchedulingPolicy>(new PrioritySchedulingPolicy());
    TaskManager mgr(std::move(policy));

    auto listener = std::make_shared<LoggingListener>();
    mgr.addListener(listener);

    auto now = std::chrono::system_clock::now();

    std::thread producer([&]{
        mgr.addTask("Design API", "Define endpoints", now + std::chrono::hours(48), "Alice", Priority::High);
        mgr.addTask("Hotfix", "Critical bug fix", now + std::chrono::hours(6), "Bob", Priority::Critical);
        mgr.addTask("Docs", "Write docs", now + std::chrono::hours(72), "Cara", Priority::Low);
        mgr.addTask("Cleanup", "Refactor code", now + std::chrono::hours(36), "Dave", Priority::Medium);
    });

    std::thread worker([&]{
        for (int i = 0; i < 4; ++i) {
            Task job = mgr.takeNextTask();
            std::cout << "Worker picked id=" << job.id << " [" << job.title << "]\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // simulate
            mgr.updateStatus(job.id, Status::Completed);
        }
    });

    producer.join();
    worker.join();

    auto all = mgr.listSortedByPriority();
    std::cout << "Total tasks: " << all.size() << "\n";
    for (const auto& t : all) {
        std::cout << "id=" << t.id
                  << " prio=" << static_cast<int>(t.priority)
                  << " status=" << (int)t.status
                  << " assignee=" << t.assignee << "\n";
    }

    return 0;
}