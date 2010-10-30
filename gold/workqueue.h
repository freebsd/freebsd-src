// workqueue.h -- the work queue for gold   -*- C++ -*-

// After processing the command line, everything the linker does is
// driven from a work queue.  This permits us to parallelize the
// linker where possible.

// Task_token
//   A simple locking implementation to ensure proper task ordering.
// Task_read_token, Task_write_token
//   Lock a Task_token for read or write.
// Task_locker
//   Task locking using RAII.
// Task
//   An abstract class for jobs to run.

#ifndef GOLD_WORKQUEUE_H
#define GOLD_WORKQUEUE_H

#include "gold-threads.h"
#include "fileread.h"

namespace gold
{

class General_options;
class Task;
class Workqueue;

// Some tasks require access to shared data structures, such as the
// symbol table.  Some tasks must be executed in a particular error,
// such as reading input file symbol tables--if we see foo.o -llib, we
// have to read the symbols for foo.o before we read the ones for
// -llib.  To implement this safely and efficiently, we use tokens.
// Task_tokens support shared read/exclusive write access to some
// resource.  Alternatively, they support blockers: blockers implement
// the requirement that some set of tasks must complete before another
// set of tasks can start.  In such a case we increment the block
// count when we create the task, and decrement it when the task
// completes.  Task_tokens are only manipulated by the main thread, so
// they do not themselves require any locking.

class Task_token
{
 public:
  Task_token();

  ~Task_token();

  // A read/write token uses these methods.

  bool
  is_readable() const;

  void
  add_reader();

  void
  remove_reader();

  bool
  is_writable() const;

  void
  add_writer(const Task*);

  void
  remove_writer(const Task*);

  bool
  has_write_lock(const Task*);

  // A blocker token uses these methods.

  void
  add_blocker();

  // Returns true if block count drops to zero.
  bool
  remove_blocker();

  bool
  is_blocked() const;

 private:
  // It makes no sense to copy these.
  Task_token(const Task_token&);
  Task_token& operator=(const Task_token&);

  bool is_blocker_;
  int readers_;
  const Task* writer_;
};

// In order to support tokens more reliably, we provide objects which
// handle them using RAII.

class Task_read_token
{
 public:
  Task_read_token(Task_token& token)
    : token_(token)
  { this->token_.add_reader(); }

  ~Task_read_token()
  { this->token_.remove_reader(); }

 private:
  Task_read_token(const Task_read_token&);
  Task_read_token& operator=(const Task_read_token&);

  Task_token& token_;
};

class Task_write_token
{
 public:
  Task_write_token(Task_token& token, const Task* task)
    : token_(token), task_(task)
  { this->token_.add_writer(this->task_); }

  ~Task_write_token()
  { this->token_.remove_writer(this->task_); }

 private:
  Task_write_token(const Task_write_token&);
  Task_write_token& operator=(const Task_write_token&);

  Task_token& token_;
  const Task* task_;
};

class Task_block_token
{
 public:
  // The blocker count must be incremented when the task is created.
  // This object is created when the task is run.  When we unblock the
  // last task, we notify the workqueue.
  Task_block_token(Task_token& token, Workqueue* workqueue);
  ~Task_block_token();

 private:
  Task_block_token(const Task_block_token&);
  Task_block_token& operator=(const Task_block_token&);

  Task_token& token_;
  Workqueue* workqueue_;
};

// An object which implements an RAII lock for any object which
// supports lock and unlock methods.

template<typename Obj>
class Task_lock_obj
{
 public:
  Task_lock_obj(Obj& obj)
    : obj_(obj)
  { this->obj_.lock(); }

  ~Task_lock_obj()
  { this->obj_.unlock(); }

 private:
  Task_lock_obj(const Task_lock_obj&);
  Task_lock_obj& operator=(const Task_lock_obj&);

  Obj& obj_;
};

// An abstract class used to lock Task_tokens using RAII.  A typical
// implementation would simply have a set of members of type
// Task_read_token, Task_write_token, and Task_block_token.

class Task_locker
{
 public:
  Task_locker()
  { }

  virtual ~Task_locker()
  { }
};

// A version of Task_locker which may be used for a single read lock.

class Task_locker_read : public Task_locker
{
 public:
  Task_locker_read(Task_token& token)
    : read_token_(token)
  { }

 private:
  Task_locker_read(const Task_locker_read&);
  Task_locker_read& operator=(const Task_locker_read&);

  Task_read_token read_token_;
};

// A version of Task_locker which may be used for a single write lock.

class Task_locker_write : public Task_locker
{
 public:
  Task_locker_write(Task_token& token, const Task* task)
    : write_token_(token, task)
  { }

 private:
  Task_locker_write(const Task_locker_write&);
  Task_locker_write& operator=(const Task_locker_write&);

  Task_write_token write_token_;
};

// A version of Task_locker which may be used for a single blocker
// lock.

class Task_locker_block : public Task_locker
{
 public:
  Task_locker_block(Task_token& token, Workqueue* workqueue)
    : block_token_(token, workqueue)
  { }

 private:
  Task_locker_block(const Task_locker_block&);
  Task_locker_block& operator=(const Task_locker_block&);

  Task_block_token block_token_;
};

// A version of Task_locker which may be used to hold a lock on any
// object which supports lock() and unlock() methods.

template<typename Obj>
class Task_locker_obj : public Task_locker
{
 public:
  Task_locker_obj(Obj& obj)
    : obj_lock_(obj)
  { }

 private:
  Task_locker_obj(const Task_locker_obj&);
  Task_locker_obj& operator=(const Task_locker_obj&);

  Task_lock_obj<Obj> obj_lock_;
};

// The superclass for tasks to be placed on the workqueue.  Each
// specific task class will inherit from this one.

class Task
{
 public:
  Task()
  { }
  virtual ~Task()
  { }

  // Type returned by Is_runnable.
  enum Is_runnable_type
  {
    // Task is runnable.
    IS_RUNNABLE,
    // Task is waiting for a block to clear.
    IS_BLOCKED,
    // Task is not waiting for a block, but is not runnable--i.e., is
    // waiting for a lock.
    IS_LOCKED
  };

  // Return whether the task can be run now.  This method is only
  // called from the main thread.
  virtual Is_runnable_type
  is_runnable(Workqueue*) = 0;

  // Return a pointer to a Task_locker which locks all the resources
  // required by the task.  We delete the pointer when the task is
  // complete.  This method can return NULL if no locks are required.
  // This method is only called from the main thread.
  virtual Task_locker*
  locks(Workqueue*) = 0;

  // Run the task.
  virtual void
  run(Workqueue*) = 0;

 private:
  Task(const Task&);
  Task& operator=(const Task&);
};

// A simple task which waits for a blocker and then runs a function.

class Task_function_runner
{
 public:
  virtual ~Task_function_runner()
  { }

  virtual void
  run(Workqueue*) = 0;
};

class Task_function : public Task
{
 public:
  // Both points should be allocated using new, and will be deleted
  // after the task runs.
  Task_function(Task_function_runner* runner, Task_token* blocker)
    : runner_(runner), blocker_(blocker)
  { }

  ~Task_function()
  {
    delete this->runner_;
    delete this->blocker_;
  }

  // The standard task methods.

  // Wait until the task is unblocked.
  Is_runnable_type
  is_runnable(Workqueue*)
  { return this->blocker_->is_blocked() ? IS_BLOCKED : IS_RUNNABLE; }

  // This type of task does not normally hold any locks.
  virtual Task_locker*
  locks(Workqueue*)
  { return NULL; }

  // Run the action.
  void
  run(Workqueue* workqueue)
  { this->runner_->run(workqueue); }

 private:
  Task_function(const Task_function&);
  Task_function& operator=(const Task_function&);

  Task_function_runner* runner_;
  Task_token* blocker_;
};

// The workqueue

class Workqueue_runner;

class Workqueue
{
 public:
  Workqueue(const General_options&);
  ~Workqueue();

  // Add a new task to the work queue.
  void
  queue(Task*);

  // Add a new task to the front of the work queue.  It will be the
  // next task to run if it is ready.
  void
  queue_front(Task*);

  // Process all the tasks on the work queue.
  void
  process();

  // A complete set of blocking tasks has completed.
  void
  cleared_blocker();

 private:
  // This class can not be copied.
  Workqueue(const Workqueue&);
  Workqueue& operator=(const Workqueue&);

  typedef std::list<Task*> Task_list;

  // Run a task.
  void run(Task*);

  friend class Workqueue_runner;

  // Find a runnable task.
  Task* find_runnable(Task_list&, bool*);

  // Add a lock to the completed queue.
  void completed(Task*, Task_locker*);

  // Clear the completed queue.
  bool clear_completed();

  // How to run a task.  Only accessed from main thread.
  Workqueue_runner* runner_;

  // Lock for access to tasks_ members.
  Lock tasks_lock_;
  // List of tasks to execute at each link level.
  Task_list tasks_;

  // Lock for access to completed_ and running_ members.
  Lock completed_lock_;
  // List of Task_locker objects for main thread to free.
  std::list<Task_locker*> completed_;
  // Number of tasks currently running.
  int running_;
  // Condition variable signalled when a new entry is added to completed_.
  Condvar completed_condvar_;

  // Number of blocker tokens which were fully cleared.  Only accessed
  // from main thread.
  int cleared_blockers_;
};

} // End namespace gold.

#endif // !defined(GOLD_WORKQUEUE_H)
