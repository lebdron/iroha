#pragma once

#include <atomic>
#include <mutex>
#include <sstream>

#include <boost/core/demangle.hpp>

static std::atomic<size_t> obj_counter_longest_class_name(0);

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
    const size_t class_name_padding_length =
        obj_counter_longest_class_name.load(std::memory_order_relaxed)
        - class_name_.size();
    std::stringstream ss;
    ss << class_name_ << ": " << std::string(class_name_padding_length, '.')
       << " created " << objects_created.load(std::memory_order_relaxed)
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
  static std::string class_name_;
};

template <typename T>
std::atomic<size_t> ObjCounter<T>::objects_created(0);
template <typename T>
std::atomic<size_t> ObjCounter<T>::objects_alive(0);
template <typename T>
std::mutex ObjCounter<T>::get_stats_mu_;
template <typename T>
std::string ObjCounter<T>::class_name_([] {
  std::string class_name = boost::core::demangle(typeid(T).name());
  const size_t my_class_name_length = class_name.size();
  size_t cur_max_length = obj_counter_longest_class_name;
  while (not std::atomic_compare_exchange_weak(
      &obj_counter_longest_class_name,
      &cur_max_length,
      std::max(my_class_name_length, cur_max_length)))
    ;
  return class_name;
}());
