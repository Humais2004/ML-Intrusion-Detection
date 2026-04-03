#ifndef BATCH_MANAGER_H
#define BATCH_MANAGER_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BatchManager {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv_consumer;  // consumers wait here when queue is empty
    std::condition_variable cv_producer;  // producers wait here when queue is full
    bool done_producing = false;
    static const size_t MAX_CAPACITY = 5;

public:
    // Pushes data into the queue safely using std::move to prevent RAM duplication
    // Blocks if the queue already has MAX_CAPACITY batches to cap memory usage
    void produce(T&& item) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_producer.wait(lock, [this]() { return queue.size() < MAX_CAPACITY; });
        queue.push(std::move(item));
        lock.unlock();
        cv_consumer.notify_one(); // Wake up any sleeping consumers (like OpenMP)
    }

    // Pulls data out. Returns false ONLY if the file is completely finished AND the queue is empty.
    bool consume(T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_consumer.wait(lock, [this]() { return !queue.empty() || done_producing; });

        if (queue.empty() && done_producing) {
            return false;
        }

        item = std::move(queue.front());
        queue.pop();
        lock.unlock();
        cv_producer.notify_one(); // Wake up producer if it was blocked on a full queue
        return true;
    }

    // The "Poison Pill" - signals that the DataLoader hit the end of the file
    void setFinished() {
        std::unique_lock<std::mutex> lock(mtx);
        done_producing = true;
        lock.unlock();
        cv_consumer.notify_all(); // Wake up everyone so they can shut down gracefully
    }
};

#endif // BATCH_MANAGER_H