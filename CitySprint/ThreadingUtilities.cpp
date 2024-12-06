#include "ThreadingUtilities.h"
#include <iostream>
#include <utility> // For std::move
#include <string>

ThreadPool::ThreadPool(size_t numThreads) : stop(false)
{
  std::cout << "Initializing Thread Pool with: " << std::to_string(numThreads) << " Worker Threads." << std::endl;
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] {
      for (;;) {
        Task task;

        {
          std::unique_lock<std::mutex> lock(this->queueMutex);
          this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty()) return;
          task = std::move(this->tasks.top());
          this->tasks.pop();
        }

        task.func();
      }
      });
  }
}

ThreadPool::~ThreadPool()
{
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread& worker : workers) worker.join();
}

void ThreadPool::enqueue(std::function<void()> task, int priority)
{
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.push({ priority, std::move(task) });
  }
  condition.notify_one();
}

Semaphore::Semaphore(int count) : count(count) {}

void Semaphore::acquire() {
  std::unique_lock<std::mutex> lock(semaphoreMutex);
  condition.wait(lock, [this] { return count > 0; });
  --count;
}

void Semaphore::release() {
  std::unique_lock<std::mutex> lock(semaphoreMutex);
  ++count;
  condition.notify_one();
}
