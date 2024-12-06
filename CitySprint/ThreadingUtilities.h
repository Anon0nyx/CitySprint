#ifndef THREADING_UTILITIES_H
#define THREADING_UTILITIES_H

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>

class ThreadPool {
public:
  ThreadPool(size_t numThreads);
  ~ThreadPool();

  void enqueue(std::function<void()> task, int priority = 0);

private:
  struct Task {
    int priority;
    std::function<void()> func;

    bool operator<(const Task& other) const {
      return priority > other.priority; // Higher priority tasks have lower values
    }
  };

  std::vector<std::thread> workers;
  std::priority_queue<Task> tasks;

  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
};

class Semaphore {
public:
  Semaphore(int count = 0);

  void acquire();
  void release();

private:
  std::mutex semaphoreMutex;
  std::condition_variable condition;
  int count;
};

#endif // THREADING_UTILITIES_H
