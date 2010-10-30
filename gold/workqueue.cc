// workqueue.cc -- the workqueue for gold

#include "gold.h"

#include "workqueue.h"

namespace gold
{

// Task_token methods.

Task_token::Task_token()
  : is_blocker_(false), readers_(0), writer_(NULL)
{
}

Task_token::~Task_token()
{
  gold_assert(this->readers_ == 0 && this->writer_ == NULL);
}

bool
Task_token::is_readable() const
{
  gold_assert(!this->is_blocker_);
  return this->writer_ == NULL;
}

void
Task_token::add_reader()
{
  gold_assert(!this->is_blocker_);
  gold_assert(this->is_readable());
  ++this->readers_;
}

void
Task_token::remove_reader()
{
  gold_assert(!this->is_blocker_);
  gold_assert(this->readers_ > 0);
  --this->readers_;
}

bool
Task_token::is_writable() const
{
  gold_assert(!this->is_blocker_);
  return this->writer_ == NULL && this->readers_ == 0;
}

void
Task_token::add_writer(const Task* t)
{
  gold_assert(!this->is_blocker_);
  gold_assert(this->is_writable());
  this->writer_ = t;
}

void
Task_token::remove_writer(const Task* t)
{
  gold_assert(!this->is_blocker_);
  gold_assert(this->writer_ == t);
  this->writer_ = NULL;
}

bool
Task_token::has_write_lock(const Task* t)
{
  gold_assert(!this->is_blocker_);
  return this->writer_ == t;
}

// For blockers, we just use the readers_ field.

void
Task_token::add_blocker()
{
  if (this->readers_ == 0 && this->writer_ == NULL)
    this->is_blocker_ = true;
  else
    gold_assert(this->is_blocker_);
  ++this->readers_;
}

bool
Task_token::remove_blocker()
{
  gold_assert(this->is_blocker_ && this->readers_ > 0);
  --this->readers_;
  return this->readers_ == 0;
}

bool
Task_token::is_blocked() const
{
  gold_assert(this->is_blocker_
	      || (this->readers_ == 0 && this->writer_ == NULL));
  return this->readers_ > 0;
}

// The Task_block_token class.

Task_block_token::Task_block_token(Task_token& token, Workqueue* workqueue)
  : token_(token), workqueue_(workqueue)
{
  // We must increment the block count when the task is created and
  // put on the queue.  This object is created when the task is run,
  // so we don't increment the block count here.
  gold_assert(this->token_.is_blocked());
}

Task_block_token::~Task_block_token()
{
  if (this->token_.remove_blocker())
    {
      // Tell the workqueue that a blocker was cleared.  This is
      // always called in the main thread, so no locking is required.
      this->workqueue_->cleared_blocker();
    }
}

// The Workqueue_runner abstract class.

class Workqueue_runner
{
 public:
  Workqueue_runner(Workqueue* workqueue)
    : workqueue_(workqueue)
  { }
  virtual ~Workqueue_runner()
  { }

  // Run a task.  This is always called in the main thread.
  virtual void run(Task*, Task_locker*) = 0;

 protected:
  // This is called by an implementation when a task is completed.
  void completed(Task* t, Task_locker* tl)
  { this->workqueue_->completed(t, tl); }

  Workqueue* get_workqueue() const
  { return this->workqueue_; }

 private:
  Workqueue* workqueue_;
};

// The simple single-threaded implementation of Workqueue_runner.

class Workqueue_runner_single : public Workqueue_runner
{
 public:
  Workqueue_runner_single(Workqueue* workqueue)
    : Workqueue_runner(workqueue)
  { }
  ~Workqueue_runner_single()
  { }

  void run(Task*, Task_locker*);
};

void
Workqueue_runner_single::run(Task* t, Task_locker* tl)
{
  t->run(this->get_workqueue());
  this->completed(t, tl);
}

// Workqueue methods.

Workqueue::Workqueue(const General_options&)
  : tasks_lock_(),
    tasks_(),
    completed_lock_(),
    completed_(),
    running_(0),
    completed_condvar_(this->completed_lock_),
    cleared_blockers_(0)
{
  // At some point we will select the specific implementation of
  // Workqueue_runner to use based on the command line options.
  this->runner_ = new Workqueue_runner_single(this);
}

Workqueue::~Workqueue()
{
  gold_assert(this->tasks_.empty());
  gold_assert(this->completed_.empty());
  gold_assert(this->running_ == 0);
}

// Add a task to the queue.

void
Workqueue::queue(Task* t)
{
  Hold_lock hl(this->tasks_lock_);
  this->tasks_.push_back(t);
}

// Add a task to the front of the queue.

void
Workqueue::queue_front(Task* t)
{
  Hold_lock hl(this->tasks_lock_);
  this->tasks_.push_front(t);
}

// Clear the list of completed tasks.  Return whether we cleared
// anything.  The completed_lock_ must be held when this is called.

bool
Workqueue::clear_completed()
{
  if (this->completed_.empty())
    return false;
  do
    {
      delete this->completed_.front();
      this->completed_.pop_front();
    }
  while (!this->completed_.empty());
  return true;
}

// Find a runnable task in TASKS, which is non-empty.  Return NULL if
// none could be found.  The tasks_lock_ must be held when this is
// called.  Sets ALL_BLOCKED if all non-runnable tasks are waiting on
// a blocker.

Task*
Workqueue::find_runnable(Task_list& tasks, bool* all_blocked)
{
  Task* tlast = tasks.back();
  *all_blocked = true;
  while (true)
    {
      Task* t = tasks.front();
      tasks.pop_front();

      Task::Is_runnable_type is_runnable = t->is_runnable(this);
      if (is_runnable == Task::IS_RUNNABLE)
	return t;

      if (is_runnable != Task::IS_BLOCKED)
	*all_blocked = false;

      tasks.push_back(t);

      if (t == tlast)
	{
	  // We couldn't find any runnable task.  If there are any
	  // completed tasks, free their locks and try again.

	  {
	    Hold_lock hl2(this->completed_lock_);

	    if (!this->clear_completed())
	      {
		// There had better be some tasks running, or we will
		// never find a runnable task.
		gold_assert(this->running_ > 0);

		// We couldn't find any runnable tasks, and we
		// couldn't release any locks.
		return NULL;
	      }
	  }

	  // We're going around again, so recompute ALL_BLOCKED.
	  *all_blocked = true;
	}
    }
}

// Process all the tasks on the workqueue.  This is the main loop in
// the linker.  Note that as we process tasks, new tasks will be
// added.

void
Workqueue::process()
{
  while (true)
    {
      Task* t;
      bool empty;
      bool all_blocked;

      {
	Hold_lock hl(this->tasks_lock_);

	if (this->tasks_.empty())
	  {
	    t = NULL;
	    empty = true;
	    all_blocked = false;
	  }
	else
	  {
	    t = this->find_runnable(this->tasks_, &all_blocked);
	    empty = false;
	  }
      }

      // If T != NULL, it is a task we can run.
      // If T == NULL && empty, then there are no tasks waiting to
      // be run at this level.
      // If T == NULL && !empty, then there tasks waiting to be
      // run at this level, but they are waiting for something to
      // unlock.

      if (t != NULL)
	this->run(t);
      else if (!empty)
	{
	  {
	    Hold_lock hl(this->completed_lock_);

	    // There must be something for us to wait for, or we won't
	    // be able to make progress.
	    gold_assert(this->running_ > 0 || !this->completed_.empty());

	    if (all_blocked)
	      {
		this->cleared_blockers_ = 0;
		this->clear_completed();
		while (this->cleared_blockers_ == 0)
		  {
		    gold_assert(this->running_ > 0);
		    this->completed_condvar_.wait();
		    this->clear_completed();
		  }
	      }
	    else
	      {
		if (this->running_ > 0)
		  {
		    // Wait for a task to finish.
		    this->completed_condvar_.wait();
		  }
		this->clear_completed();
	      }
	  }
	}
      else
	{
	  {
	    Hold_lock hl(this->completed_lock_);

	    // If there are no running tasks, then we are done.
	    if (this->running_ == 0)
	      {
		this->clear_completed();
		return;
	      }

	    // Wait for a task to finish.  Then we have to loop around
	    // again in case it added any new tasks before finishing.
	    this->completed_condvar_.wait();
	    this->clear_completed();
	  }
	}
    }
}

// Run a task.  This is always called in the main thread.

void
Workqueue::run(Task* t)
{
  ++this->running_;
  this->runner_->run(t, t->locks(this));
}

// This is called when a task is completed to put the locks on the
// list to be released.  We use a list because we only want the locks
// to be released in the main thread.

void
Workqueue::completed(Task* t, Task_locker* tl)
{
  {
    Hold_lock hl(this->completed_lock_);
    gold_assert(this->running_ > 0);
    --this->running_;
    this->completed_.push_back(tl);
    this->completed_condvar_.signal();
  }
  delete t;
}

// This is called when the last task for a blocker has completed.
// This is always called in the main thread.

void
Workqueue::cleared_blocker()
{
  ++this->cleared_blockers_;
}

} // End namespace gold.
