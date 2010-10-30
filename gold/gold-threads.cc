// gold-threads.cc -- thread support for gold

#include "gold.h"

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#include "gold-threads.h"

namespace gold
{

// Class Lock_impl. 

class Lock_impl
{
 public:
  Lock_impl();
  ~Lock_impl();

  void acquire();

  void release();

private:
  // This class can not be copied.
  Lock_impl(const Lock_impl&);
  Lock_impl& operator=(const Lock_impl&);

  friend class Condvar_impl;

#ifdef ENABLE_THREADS
  pthread_mutex_t mutex_;
#else
  bool acquired_;
#endif
};

#ifdef ENABLE_THREADS

Lock_impl::Lock_impl()
{
  pthread_mutexattr_t attr;
  if (pthread_mutexattr_init(&attr) != 0)
    gold_fatal(_("pthead_mutextattr_init failed"), true);
#ifdef PTHREAD_MUTEXT_ADAPTIVE_NP
  if (pthread_mutextattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP) != 0)
    gold_fatal(_("pthread_mutextattr_settype failed"), true);
#endif

  if (pthread_mutex_init (&this->mutex_, &attr) != 0)
    gold_fatal(_("pthread_mutex_init failed"), true);

  if (pthread_mutexattr_destroy(&attr) != 0)
    gold_fatal(_("pthread_mutexattr_destroy failed"), true);
}

Lock_impl::~Lock_impl()
{
  if (pthread_mutex_destroy(&this->mutex_) != 0)
    gold_fatal(_("pthread_mutex_destroy failed"), true);
}

void
Lock_impl::acquire()
{
  if (pthread_mutex_lock(&this->mutex_) != 0)
    gold_fatal(_("pthread_mutex_lock failed"), true);
}

void
Lock_impl::release()
{
  if (pthread_mutex_unlock(&this->mutex_) != 0)
    gold_fatal(_("pthread_mutex_unlock failed"), true);
}

#else // !defined(ENABLE_THREADS)

Lock_impl::Lock_impl()
  : acquired_(false)
{
}

Lock_impl::~Lock_impl()
{
  gold_assert(!this->acquired_);
}

void
Lock_impl::acquire()
{
  gold_assert(!this->acquired_);
  this->acquired_ = true;
}

void
Lock_impl::release()
{
  gold_assert(this->acquired_);
  this->acquired_ = false;
}

#endif // !defined(ENABLE_THREADS)

// Methods for Lock class.

Lock::Lock()
{
  this->lock_ = new Lock_impl;
}

Lock::~Lock()
{
  delete this->lock_;
}

void
Lock::acquire()
{
  this->lock_->acquire();
}

void
Lock::release()
{
  this->lock_->release();
}

// Class Condvar_impl.

class Condvar_impl
{
 public:
  Condvar_impl();
  ~Condvar_impl();

  void wait(Lock_impl*);
  void signal();

 private:
  // This class can not be copied.
  Condvar_impl(const Condvar_impl&);
  Condvar_impl& operator=(const Condvar_impl&);

#ifdef ENABLE_THREADS
  pthread_cond_t cond_;
#endif
};

#ifdef ENABLE_THREADS

Condvar_impl::Condvar_impl()
{
  if (pthread_cond_init(&this->cond_, NULL) != 0)
    gold_fatal(_("pthread_cond_init failed"), true);
}

Condvar_impl::~Condvar_impl()
{
  if (pthread_cond_destroy(&this->cond_) != 0)
    gold_fatal(_("pthread_cond_destroy failed"), true);
}

void
Condvar_impl::wait(Lock_impl* li)
{
  if (pthread_cond_wait(&this->cond_, &li->mutex_) != 0)
    gold_fatal(_("pthread_cond_wait failed"), true);
}

void
Condvar_impl::signal()
{
  if (pthread_cond_signal(&this->cond_) != 0)
    gold_fatal(_("pthread_cond_signal failed"), true);
}

#else // !defined(ENABLE_THREADS)

Condvar_impl::Condvar_impl()
{
}

Condvar_impl::~Condvar_impl()
{
}

void
Condvar_impl::wait(Lock_impl* li)
{
  gold_assert(li->acquired_);
}

void
Condvar_impl::signal()
{
}

#endif // !defined(ENABLE_THREADS)

// Methods for Condvar class.

Condvar::Condvar(Lock& lock)
  : lock_(lock)
{
  this->condvar_ = new Condvar_impl;
}

Condvar::~Condvar()
{
  delete this->condvar_;
}

void
Condvar::wait()
{
  this->condvar_->wait(this->lock_.get_impl());
}

void
Condvar::signal()
{
  this->condvar_->signal();
}

} // End namespace gold.
