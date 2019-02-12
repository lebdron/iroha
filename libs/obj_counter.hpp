#pragma once

#include <atomic>
#include <mutex>
#include <sstream>

template <typename T>
struct ObjCounter {
  static std::atomic<size_t> objects_created;
  static std::atomic<size_t> objects_alive;

  ObjCounter() noexcept {
    objects_created.fetch_add(1, std::memory_order_relaxed);
    objects_alive.fetch_add(1, std::memory_order_relaxed);
  }

  static std::string getStats() {
    std::lock_guard<std::mutex> lock(get_stats_mu_);
    std::stringstream ss;
    ss << typeid(T).name() << ": created "
       << objects_created.load(std::memory_order_relaxed)
       << ", alive :" << objects_alive.load(std::memory_order_relaxed);
    return ss.str();
  }

 protected:
  ~ObjCounter()  // objects should never be removed through pointers of this type
  {
    objects_alive.fetch_add(-1, std::memory_order_relaxed);
  }

 private:
  static std::mutex get_stats_mu_;
};

template <typename T>
std::atomic<size_t> ObjCounter<T>::objects_created(0);
template <typename T>
std::atomic<size_t> ObjCounter<T>::objects_alive(0);
template <typename T>
std::mutex ObjCounter<T>::get_stats_mu_;
