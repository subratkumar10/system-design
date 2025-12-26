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

// Strategy: choose next pending task (priority-based)
struct ISchedulingPolicy {
    virtual ~ISchedulingPolicy() {}
    virtual int selectNextPending(const std::vector<Task>& snapshot) const = 0;
};

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

// Factory: atomic ID generation
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
        t.assignee = assignee;
        t.priority = prio;
        t.status = Status::Pending;
        return t;
    }
private:
    std::atomic<int> nextId_;
};

class TaskManager {
public:
    TaskManager()
        : policy_(new PrioritySchedulingPolicy()) {}

    // Optional: inject a custom policy
    explicit TaskManager(std::unique_ptr<ISchedulingPolicy> policy)
        : policy_(std::move(policy)) {}

    int addTask(const std::string& title,
                const std::string& description,
                const std::chrono::system_clock::time_point& due,
                const std::string& assignee,
                Priority priority) {
        Task t = factory_.create(title, description, due, assignee, priority);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_[t.id] = t;
        }
        cv_.notify_one();
        return t.id;
    }

    bool updateStatus(int id, Status newStatus) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        it->second.status = newStatus;
        if (newStatus == Status::Pending) cv_.notify_one();
        return true;
    }

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
        if (it->second.status == Status::Pending) cv_.notify_one();
        return true;
    }

    bool getTask(int id, Task& out) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<Task> listSortedByPriority() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<Task> v;
        v.reserve(tasks_.size());
        for (const auto& kv : tasks_) v.push_back(kv.second);
        std::sort(v.begin(), v.end(), [](const Task& a, const Task& b) {
            if (static_cast<int>(a.priority) != static_cast<int>(b.priority))
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            if (a.due != b.due) return a.due < b.due;
            return a.id < b.id;
        });
        return v;
    }

    // Non-throwing timeout: returns true if a task is taken, false on timeout or contention
    bool takeNextTask(Task& out, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cv_.wait_for(lock, timeout, [&]{
            return shuttingDown_.load(std::memory_order_relaxed) || hasPendingUnsafe();
        })) {
            return false; // timed out
        }
        if (shuttingDown_.load(std::memory_order_relaxed)) return false;

        // snapshot for selection using policy
        std::vector<Task> snap;
        snap.reserve(tasks_.size());
        for (const auto& kv : tasks_) snap.push_back(kv.second);
        int idx = policy_->selectNextPending(snap);
        if (idx < 0) return false; // spurious wake/contended

        int chosenId = snap[idx].id;
        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) return false;

        it->second.status = Status::InProgress;
        out = it->second;
        return true;
    }

    // Throwing timeout: throws on timeout or shutdown
    Task takeNextTaskOrThrow(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!cv_.wait_for(lock, timeout, [&]{
            return shuttingDown_.load(std::memory_order_relaxed) || hasPendingUnsafe();
        })) {
            throw std::runtime_error("timeout waiting for a pending task");
        }
        if (shuttingDown_.load(std::memory_order_relaxed)) {
            throw std::runtime_error("manager shutting down");
        }

        std::vector<Task> snap;
        snap.reserve(tasks_.size());
        for (const auto& kv : tasks_) snap.push_back(kv.second);
        int idx = policy_->selectNextPending(snap);
        if (idx < 0) throw std::runtime_error("no pending task after wakeup");

        int chosenId = snap[idx].id;
        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) {
            throw std::runtime_error("task contention after wakeup");
        }

        it->second.status = Status::InProgress;
        return it->second;
    }

    // Graceful shutdown: wakes all waiters, causing timeouts or early returns
    void shutdown() {
        shuttingDown_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
    }

private:
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
    std::atomic<bool> shuttingDown_{false};
};

// Demo
int main() {
    TaskManager mgr;
    auto now = std::chrono::system_clock::now();

    // Producer posts a task after 1 second
    std::thread producer([&]{
        std::this_thread::sleep_for(std::chrono::seconds(1));
        mgr.addTask("Hotfix", "Critical issue", now + std::chrono::hours(1), "Bob", Priority::Critical);
    });

    // Worker tries to get a task with 500ms timeout, retrying a few times
    std::thread worker([&]{
        for (int tries = 0; tries < 5; ++tries) {
            Task job;
            if (mgr.takeNextTask(job, std::chrono::milliseconds(500))) {
                std::cout << "Worker got task id=" << job.id << " (" << job.title << ")\n";
                mgr.updateStatus(job.id, Status::Completed);
                return;
            } else {
                std::cout << "Worker: timeout waiting for task (try " << tries+1 << ")\n";
            }
        }
        std::cout << "Worker: giving up and shutting down\n";
        mgr.shutdown();
    });

    producer.join();
    worker.join();

    auto all = mgr.listSortedByPriority();
    std::cout << "Tasks total: " << all.size() << "\n";
    for (auto& t : all) {
        std::cout << "id=" << t.id
                  << " prio=" << static_cast<int>(t.priority)
                  << " status=" << static_cast<int>(t.status)
                  << " assignee=" << t.assignee << "\n";
    }
    return 0;
}