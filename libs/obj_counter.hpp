#pragma once

#include <atomic>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <execinfo.h>
#include <fmt/core.h>
#include <boost/core/demangle.hpp>

static std::atomic<size_t> obj_counter_longest_class_name(0);

struct AllCountedStats {
  using GetStatsFn = void (*)(const std::string &);
  AllCountedStats(GetStatsFn f) {
    std::lock_guard<std::mutex> lock(mu_);
    get_stats_.emplace_back(f);
  }

  // for this object to be ever used
  void useMe() const {}

  static void getAllStats(const std::string &file_path_format) {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto &f : get_stats_) {
      (*f)(file_path_format);
    }
  }

 private:
  static std::mutex mu_;
  static std::vector<GetStatsFn> get_stats_;
};

class ConstrBt {
 public:
  ConstrBt() {
    // void *addresses[kMaxBtSize];

    bt_size_ = backtrace(bt_addresses_, kMaxBtSize);
    // backtrace_ = backtrace_symbols(addresses, bt_size_);
  }

  void getBt(std::ostream &os) {
    os << std::hex;
    for (size_t i = 0; i < bt_size_; ++i) {
      os << bt_addresses_[i] << std::endl;
    }
  }

 private:
  static constexpr size_t kMaxBtSize = 100;

  // char **backtrace_;
  void *bt_addresses_[kMaxBtSize];
  size_t bt_size_{0};
};

template <typename T>
struct ObjCounter : public ConstrBt {
  ObjCounter() noexcept {
    std::lock_guard<std::mutex> lock(counter_mu_);
    object_id_ = objects_created++;
    objects_alive_.emplace(std::make_pair(object_id_, this));
    all_stats_register_.useMe();
  }

  static void getStats(const std::string &file_path_format) {
    auto file_path = fmt::format(file_path_format, class_name_);
    std::ofstream of(file_path);
    assert(of.good());
    {
      std::lock_guard<std::mutex> lock(counter_mu_);
      const size_t class_name_padding_length =
          obj_counter_longest_class_name.load(std::memory_order_relaxed)
          - class_name_.size();
      of << class_name_ << ": " << std::string(class_name_padding_length, '.')
         << " created " << objects_created
         << ", alive :" << objects_alive_.size();

      of << std::endl << "Living objects' backtraces:" << std::endl;
      for (const auto &id_and_ptr : objects_alive_) {
        of << std::dec << id_and_ptr.first << ":" << std::endl << std::hex;
        id_and_ptr.second->getBt(of);
      }
    }
    of << std::endl
       << "End of " << class_name_ << " living objects' backtraces."
       << std::endl;
    of.close();
  }

 protected:
  ~ObjCounter()  // objects should never be removed through pointers of this
                 // type
  {
    std::lock_guard<std::mutex> lock(counter_mu_);
    objects_alive_.erase(object_id_);
  }

 private:
  static std::string class_name_;
  static const AllCountedStats all_stats_register_;

  static std::mutex counter_mu_;
  static std::map<size_t, ConstrBt *> objects_alive_;
  static size_t objects_created;
  size_t object_id_;
};

template <typename T>
class UniquePtrCounter : public ObjCounter<UniquePtrCounter<T>> {
 public:
  constexpr UniquePtrCounter() noexcept : std::unique_ptr<T>() {}

  template <typename... Types,
            typename = std::enable_if_t<
                std::is_constructible<std::unique_ptr<T>, Types &&...>::value>>
  UniquePtrCounter(Types &&... args) noexcept
      : ptr_(std::forward<Types>(args)...) {}

  template <typename U,
            typename = std::enable_if_t<
                std::is_constructible<std::unique_ptr<T>,
                                      std::unique_ptr<U> &&>::value>>
  UniquePtrCounter(UniquePtrCounter<U> &&o) noexcept
      : ptr_(std::move(o.ptr_)) {}

  /*
  explicit UniquePtrCounter(T *p) noexcept : std::unique_ptr<T>(p) {}

  UniquePtrCounter(std::unique_ptr<T> &&p) noexcept
      : std::unique_ptr<T>(std::move(p)) {}

  template <typename U, typename E>
  UniquePtrCounter(std::unique_ptr<U, E> &&p) noexcept
      : std::unique_ptr<T>(std::unique_ptr<U, E>(std::move(p))) {}
  */

  template <
      typename U,
      typename = std::enable_if_t<std::is_same<
          decltype(std::declval<std::unique_ptr<T>>() = std::declval<U &&>()),
          std::unique_ptr<T> &>::value>>
  UniquePtrCounter &operator=(U &&o) noexcept {
    ptr_ = (std::forward<U>(o));
  }

  /*
  template <typename U, typename E>
  UniquePtrCounter &operator=(std::unique_ptr<U, E> &&p) noexcept {
    std::unique_ptr<U, E>::operator=(std::move(p));
  }

  UniquePtrCounter &operator=(UniquePtrCounter<T> &&p) noexcept {
    std::unique_ptr<T>::operator=(std::move(p));
  }
  */

  T *get() const noexcept {
    return ptr_.get();
  }

  typename std::add_lvalue_reference<T>::type operator*() const {
    return *ptr_;
  }

  T *operator->() const noexcept {
    return ptr_.operator->();
  }

  operator bool() const {
    return ptr_.get() != nullptr;
  }

  T *release() {
    return ptr_.release();
  }

  std::unique_ptr<T> ptr_;
};

template <typename T>
class SharedPtrCounter : public ObjCounter<SharedPtrCounter<T>> {
 public:
  using element_type = T;

  template <typename... Types,
            typename = std::enable_if_t<
                std::is_constructible<std::shared_ptr<T>, Types &&...>::value>>
  constexpr SharedPtrCounter(Types &&... args) noexcept
      : ptr_(std::forward<Types>(args)...) {}

  // template <class Y>
  // SharedPtrCounter(const SharedPtrCounter<Y> &p, T *o) noexcept : ptr_(p, o)
  // {}

  template <class U,
            typename = std::enable_if_t<
                std::is_constructible<std::shared_ptr<T>,
                                      std::unique_ptr<U> &&>::value>>
  SharedPtrCounter(UniquePtrCounter<U> &&o) noexcept
      : ptr_(std::move(o.ptr_)) {}

  template <class U,
            typename = std::enable_if_t<
                std::is_constructible<std::shared_ptr<T>,
                                      std::shared_ptr<U> &&>::value>>
  SharedPtrCounter(SharedPtrCounter<U> &&o) noexcept
      : ptr_(std::move(o.ptr_)) {}

  template <class U,
            typename = std::enable_if_t<
                std::is_constructible<std::shared_ptr<T>,
                                      std::shared_ptr<U> const &>::value>>
  SharedPtrCounter(SharedPtrCounter<U> const &o) noexcept : ptr_(o.ptr_) {}

  /*
  constexpr SharedPtrCounter() noexcept : std::shared_ptr<T>() {}

  constexpr SharedPtrCounter(std::nullptr_t) noexcept : std::shared_ptr<T>() {}

  template <class Y>
  explicit SharedPtrCounter(Y *p) : std::shared_ptr<T>(p) {}

  template <class Y, class Deleter>
  SharedPtrCounter(Y *p, Deleter d) : std::shared_ptr<T>(p, std::move(d)) {}

  template <class Deleter>
  SharedPtrCounter(std::nullptr_t, Deleter d)
      : std::shared_ptr<T>(nullptr, std::move(d)) {}

  template <class Y, class Deleter, class Alloc>
  SharedPtrCounter(Y *p, Deleter d, Alloc alloc)
      : std::shared_ptr<T>(p, std::move(d), std::move(alloc)) {}

  template <class Deleter, class Alloc>
  SharedPtrCounter(std::nullptr_t, Deleter d, Alloc alloc)
      : std::shared_ptr<T>(nullptr, std::move(d), std::move(alloc)) {}

  template <class Y>
  SharedPtrCounter(const std::shared_ptr<Y> &p, T *o) noexcept
      : std::shared_ptr<T>(p, o) {}

  SharedPtrCounter(const std::shared_ptr<T> &p) noexcept
      : std::shared_ptr<T>(p) {}

  template <class Y>
  SharedPtrCounter(const std::shared_ptr<Y> &p) noexcept
      : std::shared_ptr<T>(p) {}

  SharedPtrCounter(std::shared_ptr<T> &&p) noexcept
      : std::shared_ptr<T>(std::move(p)) {}

  template <class Y>
  SharedPtrCounter(std::shared_ptr<Y> &&p) noexcept
      : std::shared_ptr<T>(std::move(p)) {}

  template <class Y>
  SharedPtrCounter(const SharedPtrCounter<Y> &p, T *o) noexcept
      : std::shared_ptr<T>(p, o) {}

  SharedPtrCounter(const SharedPtrCounter &p) noexcept
      : std::shared_ptr<T>(p) {}

  template <class Y>
  SharedPtrCounter(const SharedPtrCounter<Y> &p) noexcept
      : std::shared_ptr<T>(p) {}

  SharedPtrCounter(SharedPtrCounter &&p) noexcept
      : std::shared_ptr<T>(std::move(p)) {}

  template <class Y>
  SharedPtrCounter(SharedPtrCounter<Y> &&p) noexcept
      : std::shared_ptr<T>(std::move(p)) {}

  template <class Y>
  explicit SharedPtrCounter(const std::weak_ptr<Y> &p)
      : std::shared_ptr<T>(p) {}

  SharedPtrCounter(std::unique_ptr<T> &&p) : std::shared_ptr<T>(std::move(p)) {}
  */

  static constexpr char const *kMagic = "qwer";

  template <class U,
            typename = std::enable_if_t<U::kMagic == kMagic>,
            typename = std::enable_if_t<std::is_same<
                decltype(std::declval<std::shared_ptr<T>>() =
                             std::declval<std::shared_ptr<U> &&>()),
                std::shared_ptr<T> &>::value>>
  SharedPtrCounter &operator=(U &&o) {
    ptr_ = std::forward<U>(o.ptr_);
    return *this;
  }

  template <class U,
            typename = std::enable_if_t<std::is_same<
                decltype(std::declval<std::shared_ptr<T>>() =
                             std::declval<std::unique_ptr<U> &&>()),
                std::shared_ptr<T> &>::value>>
  SharedPtrCounter &operator=(UniquePtrCounter<U> &&o) {
    ptr_ = std::move(o.ptr_);
    return *this;
  }

  bool operator==(const SharedPtrCounter<T> &o) const {
    return ptr_ == o.ptr_;
  }

  T *get() const noexcept {
    return ptr_.get();
  }

  typename std::add_lvalue_reference<T>::type operator*() const {
    return *ptr_;
  }

  T *operator->() const noexcept {
    return ptr_.operator->();
  }

  operator bool() const {
    return ptr_.get() != nullptr;
  }

  std::shared_ptr<T> ptr_;
};

template <typename T, typename... Types>
SharedPtrCounter<T> makeSharedCounted(Types &&... args) {
  return SharedPtrCounter<T>(std::make_shared<T>(std::forward<Types>(args)...));
}

template <typename T, typename... Types>
UniquePtrCounter<T> makeUniqueCounted(Types &&... args) {
  return UniquePtrCounter<T>(std::make_unique<T>(std::forward<Types>(args)...));
}

template <typename T>
size_t ObjCounter<T>::objects_created(0);
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
template <typename T>
const AllCountedStats ObjCounter<T>::all_stats_register_(
    &ObjCounter<T>::getStats);
template <typename T>
std::mutex ObjCounter<T>::counter_mu_;
template <typename T>
std::map<size_t, ConstrBt *> ObjCounter<T>::objects_alive_;
