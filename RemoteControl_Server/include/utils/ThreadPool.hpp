#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <objbase.h> // COM 初始化: CoInitializeEx / CoUninitialize
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>


class ThreadPool {
public:
  // 构造函数 — 内联实现，避免外部定义导致的解析问题
  ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back([this] {
        // [关键修复] 在工作线程中初始化COM组件
        // 使用多线程模式，确保CreateStreamOnHGlobal等COM API正常工作
        CoInitializeEx(NULL, COINIT_MULTITHREADED);

        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            this->condition.wait(
                lock, [this] { return this->stop || !this->tasks.empty(); });
            if (this->stop && this->tasks.empty())
              break; // 改为break以便执行COM清理
            task = std::move(this->tasks.front());
            this->tasks.pop();
          }
          task();
        }

        // [关键修复] 线程退出前清理COM
        CoUninitialize();
      });
    }
  }

  // 任务入队函数模板
  template <class F, class... Args>
  auto enqueue(F &&f, Args &&...args)
      -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      if (stop)
        throw std::runtime_error("enqueue on stopped ThreadPool");
      tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
  }

  // 析构函数 — 内联实现
  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
      if (worker.joinable())
        worker.join();
  }

private:
  // 线程容器
  std::vector<std::thread> workers;
  // 任务队列
  std::queue<std::function<void()>> tasks;
  // 线程同步相关成员
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};
