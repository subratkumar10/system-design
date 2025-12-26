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

class TaskManager {
public:
    TaskManager() : nextId_(1) {}

    int addTask(const std::string& title,
                const std::string& description,
                const std::chrono::system_clock::time_point& due,
                const std::string& assignee,
                Priority priority) {
        Task t;
        t.id = nextId_.fetch_add(1, std::memory_order_relaxed);
        t.title = title;
        t.description = description;
        t.due = due;
        t.status = Status::Pending;
        t.assignee = assignee;
        t.priority = priority;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_[t.id] = t;
        }
        cv_.notify_one();
        return t.id;
    }

    bool deleteTask(int id) {
        std::lock_guard<std::mutex> lock(mtx_);
        return tasks_.erase(id) > 0;
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

    bool updateStatus(int id, Status newStatus) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) return false;
        it->second.status = newStatus;
        if (newStatus == Status::Pending) cv_.notify_one();
        return true;
    }

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
        for (const auto& kv : tasks_) {
            if (kv.second.assignee == who) v.push_back(kv.second);
        }
        return v;
    }

    Task takeNextTask() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]{
            for (const auto& kv : tasks_) {
                if (kv.second.status == Status::Pending) return true;
            }
            return false;
        });

        int chosenId = -1;
        Task chosen;
        for (const auto& kv : tasks_) {
            const Task& t = kv.second;
            if (t.status != Status::Pending) continue;
            if (chosenId == -1 ||
                static_cast<int>(t.priority) > static_cast<int>(chosen.priority) ||
                (t.priority == chosen.priority && (t.due < chosen.due ||
                (t.due == chosen.due && t.id < chosen.id)))) {
                chosenId = kv.first;
                chosen = t;
            }
        }
        if (chosenId == -1) {
            // Spurious wakeup: loop again
            lock.unlock();
            return takeNextTask();
        }

        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) {
            lock.unlock();
            return takeNextTask();
        }
        it->second.status = Status::InProgress;
        return it->second;
    }

    bool tryTakeNextTask(Task& out) {
        std::lock_guard<std::mutex> lock(mtx_);
        int chosenId = -1;
        Task chosen;
        for (const auto& kv : tasks_) {
            const Task& t = kv.second;
            if (t.status != Status::Pending) continue;
            if (chosenId == -1 ||
                static_cast<int>(t.priority) > static_cast<int>(chosen.priority) ||
                (t.priority == chosen.priority && (t.due < chosen.due ||
                (t.due == chosen.due && t.id < chosen.id)))) {
                chosenId = kv.first;
                chosen = t;
            }
        }
        if (chosenId == -1) return false;
        auto it = tasks_.find(chosenId);
        if (it == tasks_.end() || it->second.status != Status::Pending) return false;
        it->second.status = Status::InProgress;
        out = it->second;
        return true;
    }

    void notifyAll() { cv_.notify_all(); }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::map<int, Task> tasks_;
    std::atomic<int> nextId_;
};

// Demo usage
int main() {
    TaskManager mgr;
    auto now = std::chrono::system_clock::now();

    std::thread producer([&]{
        mgr.addTask("Design API", "Define endpoints", now + std::chrono::hours(48), "Alice", Priority::High);
        mgr.addTask("Hotfix", "Patch critical bug", now + std::chrono::hours(6), "Bob", Priority::Critical);
        mgr.addTask("Docs", "Write user guide", now + std::chrono::hours(72), "Cara", Priority::Low);
    });

    std::thread worker([&]{
        for (int i = 0; i < 3; ++i) {
            Task job = mgr.takeNextTask();
            std::cout << "Worker picked id=" << job.id << " [" << job.title << "]\n";
            // Simulate processing...
            mgr.updateStatus(job.id, Status::Completed);
        }
    });

    producer.join();
    worker.join();

    auto all = mgr.listSortedByPriority();
    std::cout << "Total tasks: " << all.size() << "\n";
    for (const auto& t : all) {
        std::cout << "id=" << t.id << " prio=" << static_cast<int>(t.priority)
                  << " status=" << static_cast<int>(t.status) << " assignee=" << t.assignee << "\n";
    }

    return 0;
}