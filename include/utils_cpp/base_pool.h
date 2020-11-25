/**
 * Created on: 8/27/16
 *     Author: xing
 */

#pragma once

#include <atomic>
#include <mutex>
#include <functional>

#include "utils_cpp/common.h"
#include "utils_cpp/module.h"

namespace utils
{
typedef std::function<void(void *)> PoolResDeleter;

/*
 * TODO(HX)
 *    Instead of implicitly force the pooled Class
 *    to implement Init/Reinit/Uninit, we can also ask for
 *    lambda to do these things.
 */

/**
 * Pointer wrapper
 * @note Smart pointer in STD is prefered usually,
 * 		This class if for situation when STD smart pointer is not
 * available, e.g. in lambda capture
 */
template <typename T>
class PooledPtr : public StringAble
{
 public:
  PooledPtr() = default;

  PooledPtr(T *ptr, const PoolResDeleter &del) : ptr{ptr}, del{del} {}

  PooledPtr(const PooledPtr &other) : PooledPtr(other.ptr, other.del)
  {
    auto &ot = const_cast<PooledPtr &>(other);
    ot.ptr = nullptr;
    ot.del = nullptr;
  }

  PooledPtr(PooledPtr &&other) noexcept : PooledPtr(other.ptr, other.del)
  {
    other.ptr = nullptr;
    other.del = nullptr;
  }

  ~PooledPtr() { recycle(); }

  PooledPtr &
  operator=(const PooledPtr &other)
  {
    auto &ot = const_cast<PooledPtr &>(other);
    recycle();
    ptr = ot.ptr;
    del = ot.del;
    ot.ptr = nullptr;
    ot.del = nullptr;
    return *this;
  }

  PooledPtr &
  operator=(PooledPtr &&other) noexcept
  {
    recycle();
    ptr = other.ptr;
    del = other.del;
    other.ptr = nullptr;
    other.del = nullptr;
    return *this;
  }

  T *
  get() const noexcept
  {
    return ptr;
  }

  std::string
  to_s() const noexcept override
  {
    std::stringstream s;
    s << "ptr:" << ptr;
    return s.str();
  }

 private:
  inline void
  recycle() noexcept
  {
    if (ptr && del) {
      del(ptr);
      ptr = nullptr;
    }
  }

  T *ptr{};
  PoolResDeleter del{};
};

/**
 * BasePool
 * @note Class T should have these methods implemented:
 * 		Init(void), which takes no argument, and returns Ret,
 * 		Ret Reinit(...), which returns Ret,
 * 		Uninit(void), which takes no argument, and returns void.
 *
 * 		This pool is not thread safe.
 * 		Try BaseCPool in multi-thread circumstances
 */
template <typename T>
class BasePool : public Module
{
 public:
  UTILS_DISALLOW_COPY_AND_ASSIGN(BasePool)

  typedef std::unique_ptr<T, PoolResDeleter> unique_ptr;
  typedef std::shared_ptr<T> shared_ptr;
  typedef PooledPtr<T> pooled_ptr;

  explicit BasePool(size_t size) : BasePool(size, "") {}

  BasePool(size_t size, const std::string &name) : Module{name}, size{size}
  {
    UTILS_RAISE_IF(size <= 0)

    size_t sz = UTILS_ROUND(sizeof(T) + sizeof(nodeptr), sizeof(nodeptr));
    mem = new uint8_t[size * sz];
    UTILS_RAISE_UNLESS(mem)

    free_mem = (nodeptr)mem;

    sz /= sizeof(nodeptr);
    nodeptr tmp = free_mem;
    while (--size) {
      tmp->next = tmp + sz;
      tmp = tmp->next;
    }

    tmp->next = nullptr;

    tmp = free_mem;
    T *t;
    while (tmp) {
      t = (T *)(tmp + 1);
      t->Init();
      tmp = tmp->next;
    }
  }

  ~BasePool() override
  {
    put_stat();
    nodeptr tmp = free_mem;
    T *t;
    while (tmp) {
      t = (T *)(tmp + 1);
      t->Uninit();

      tmp = tmp->next;
    }

    delete[] mem;
  }

  /**
   * get a unique_ptr to a T instance
   */
  template <typename... Args>
  inline std::unique_ptr<T, PoolResDeleter>
  alloc_unique(Args &&... args) noexcept
  {
    return std::unique_ptr<T, std::function<void(void *)>>{
        Alloc(std::forward<Args>(args)...),
        // FIXME(HX) Seems like it's not possible to extract
        // this lambda back into a member, as which may cause
        // a compiler error
        [this](void *b) {
          unless(b) { return; }
          auto tmp = ((nodeptr)b) - 1;
          tmp->next = free_mem;
          free_mem = tmp;
        }};
  }

  /**
   * get a shared_ptr to a T instance
   */
  template <typename... Args>
  inline std::shared_ptr<T>
  alloc_shared(Args &&... args) noexcept
  {
    return std::shared_ptr<T>{Alloc(std::forward<Args>(args)...),
                              [this](void *b) {
                                unless(b) { return; }
                                auto tmp = ((nodeptr)b) - 1;
                                tmp->next = free_mem;
                                free_mem = tmp;
                              }};
  }

  template <typename... Args>
  inline PooledPtr<T>
  AllocPooled(Args &&... args) noexcept
  {
    return PooledPtr<T>{Alloc(std::forward<Args>(args)...), [this](void *b) {
                          unless(b) { return; }
                          auto tmp = ((nodeptr)b) - 1;
                          tmp->next = free_mem;
                          free_mem = tmp;
                        }};
  }

  std::string
  to_s() const noexcept override
  {
    return get_name() + stat.to_s();
  }

 private:
  /**
   * Node head for internal memory link
   */
  struct NodeHead {
    NodeHead() = default;

    explicit NodeHead(void *ptr) : next{(NodeHead *)ptr} {}

    NodeHead *next{};
  };

  typedef NodeHead *nodeptr;

  /**
   * extracting for type T
   */
  template <typename... Args>
  T *
  Alloc(Args &&... args) noexcept
  {
    ++stat.total;

    unless(free_mem) { return nullptr; }

    auto b = (T *)(free_mem + 1);
    free_mem = free_mem->next;

    auto ret = b->Reinit(std::forward<Args>(args)...);
    if (Ret::OK != ret) {
      lErr("Reinit: " << to_string(ret)) free_mem = ((nodeptr)b) - 1;
      return nullptr;
    }

    ++stat.succ;
    return b;
  }

  struct Stat {
    inline std::string
    to_s() const noexcept
    {
      UTILS_STR_S(32)
      UTILS_STR_ATTR(total)
      UTILS_STR_ATTR(succ)
      return s;
    }

    uint64_t total;
    uint64_t succ;
  } stat;

  UTILS_READER(size_t, size);
  uint8_t *mem;
  nodeptr free_mem;
};

/**
 * BaseCPool
 * @note Class T should have these methods implemented:
 * 		Init(void), which takes no argument, and returns Ret,
 * 		Ret Reinit(...), which returns Ret,
 * 		Uninit(void), which takes no argument, and returns void.
 *
 * 		This pool is thread safe.
 */
template <typename T>
class BaseCPool : public Module
{
 public:
  UTILS_DISALLOW_COPY_AND_ASSIGN(BaseCPool)

  typedef std::unique_ptr<T, PoolResDeleter> unique_ptr;
  typedef std::shared_ptr<T> shared_ptr;
  typedef PooledPtr<T> pooled_ptr;

  explicit BaseCPool(size_t size) : BaseCPool(size, "") {}

  BaseCPool(size_t size, const std::string &name) : Module{name}, size{size}
  {
    UTILS_RAISE_IF(size <= 0)

    size_t sz = UTILS_ROUND(sizeof(T) + sizeof(nodeptr), sizeof(nodeptr));
    mem = new uint8_t[size * sz];
    UTILS_RAISE_UNLESS(mem)

    free_mem = (nodeptr)mem;

    sz /= sizeof(nodeptr);
    nodeptr tmp = free_mem;
    while (--size) {
      tmp->next = tmp + sz;
      tmp = tmp->next;
    }

    tmp->next = nullptr;

    tmp = free_mem;
    T *t;
    while (tmp) {
      t = (T *)(tmp + 1);
      t->Init();
      tmp = tmp->next;
    }
  }

  ~BaseCPool() override
  {
    put_stat();
    nodeptr tmp = free_mem;
    T *t;
    while (tmp) {
      t = (T *)(tmp + 1);
      t->Uninit();

      tmp = tmp->next;
    }

    delete[] mem;
  }

  /**
   * get a unique_ptr to a T instance
   */
  template <typename... Args>
  inline std::unique_ptr<T, PoolResDeleter>
  alloc_unique(Args &&... args) noexcept
  {
    return std::unique_ptr<T, std::function<void(void *)>>{
        Alloc(std::forward<Args>(args)...), [this](void *b) {
          unless(b) { return; }
          auto tmp = ((nodeptr)b) - 1;
          {
            std::lock_guard<std::mutex> bar{mut};
            tmp->next = free_mem;
            free_mem = tmp;
          }
        }};
  }

  /**
   * get a shared_ptr to a T instance
   */
  template <typename... Args>
  inline std::shared_ptr<T>
  alloc_shared(Args &&... args) noexcept
  {
    return std::shared_ptr<T>{Alloc(std::forward<Args>(args)...),
                              [this](void *b) {
                                unless(b) { return; }
                                auto tmp = ((nodeptr)b) - 1;
                                {
                                  std::lock_guard<std::mutex> bar{mut};
                                  tmp->next = free_mem;
                                  free_mem = tmp;
                                }
                              }};
  }

  template <typename... Args>
  inline PooledPtr<T>
  AllocPooled(Args &&... args) noexcept
  {
    return PooledPtr<T>{Alloc(std::forward<Args>(args)...), [this](void *b) {
                          unless(b) { return; }
                          auto tmp = ((nodeptr)b) - 1;
                          {
                            std::lock_guard<std::mutex> bar{mut};
                            tmp->next = free_mem;
                            free_mem = tmp;
                          }
                        }};
  }

  std::string
  to_s() const noexcept override
  {
    return get_name() + stat.to_s();
  }

 private:
  /**
   * Node head for internal memory link
   */
  struct NodeHead {
    NodeHead() = default;

    explicit NodeHead(void *ptr) : next{(NodeHead *)ptr} {}

    NodeHead *next{};
  };

  typedef NodeHead *nodeptr;

  /**
   * extracting for type T
   */
  template <typename... Args>
  T *
  Alloc(Args &&... args) noexcept
  {
    ++stat.total;

    unless(free_mem) { return nullptr; }

    {
      std::lock_guard<std::mutex> bar{mut};

      auto b = (T *)(free_mem + 1);
      free_mem = free_mem->next;

      auto ret = b->Reinit(std::forward<Args>(args)...);
      if (Ret::OK != ret) {
        lErr("Reinit: " << to_string(ret)) free_mem = ((nodeptr)b) - 1;
        return nullptr;
      }

      ++stat.succ;
      return b;
    }
  };

  struct Stat {
    inline std::string
    to_s() const noexcept
    {
      UTILS_STR_S(32)
      UTILS_STR_ATTR(total)
      UTILS_STR_ATTR(succ)
      return s;
    }

    uint64_t total{};
    uint64_t succ{};
  } stat{};

  UTILS_READER(size_t, size);
  uint8_t *mem;
  nodeptr free_mem;
  std::mutex mut{};
};

}  // namespace utils