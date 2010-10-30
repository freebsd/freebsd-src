// gold-threads.h -- thread support for gold  -*- C++ -*-

// gold can be configured to support threads.  If threads are
// supported, the user can specify at runtime whether or not to
// support them.  This provides an interface to manage locking
// accordingly.

// Lock
//   A simple lock class.

#ifndef GOLD_THREADS_H
#define GOLD_THREADS_H

namespace gold
{

class Lock_impl;
class Condvar;

// A simple lock class.

class Lock
{
 public:
  Lock();
  ~Lock();

  // Acquire the lock.
  void
  acquire();

  // Release the lock.
  void
  release();

 private:
  // This class can not be copied.
  Lock(const Lock&);
  Lock& operator=(const Lock&);

  friend class Condvar;
  Lock_impl*
  get_impl() const
  { return this->lock_; }

  Lock_impl* lock_;
};

// RAII for Lock.

class Hold_lock
{
 public:
  Hold_lock(Lock& lock)
    : lock_(lock)
  { this->lock_.acquire(); }

  ~Hold_lock()
  { this->lock_.release(); }

 private:
  // This class can not be copied.
  Hold_lock(const Hold_lock&);
  Hold_lock& operator=(const Hold_lock&);

  Lock& lock_;
};

class Condvar_impl;

// A simple condition variable class.  It is always associated with a
// specific lock.

class Condvar
{
 public:
  Condvar(Lock& lock);
  ~Condvar();

  // Wait for the condition variable to be signalled.  This should
  // only be called when the lock is held.
  void
  wait();

  // Signal the condition variable.  This should only be called when
  // the lock is held.
  void
  signal();

 private:
  // This class can not be copied.
  Condvar(const Condvar&);
  Condvar& operator=(const Condvar&);

  Lock& lock_;
  Condvar_impl* condvar_;
};

} // End namespace gold.

#endif // !defined(GOLD_THREADS_H)
