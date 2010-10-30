// common.h -- handle common symbols for gold   -*- C++ -*-

#ifndef GOLD_COMMON_H
#define GOLD_COMMON_H

#include "workqueue.h"

namespace gold
{

class General_options;
class Symbol_table;

// This task is used to allocate the common symbols.

class Allocate_commons_task : public Task
{
 public:
  Allocate_commons_task(const General_options& options, Symbol_table* symtab,
			Layout* layout, Task_token* symtab_lock,
			Task_token* blocker)
    : options_(options), symtab_(symtab), layout_(layout),
      symtab_lock_(symtab_lock), blocker_(blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  class Allocate_commons_locker;

  const General_options& options_;
  Symbol_table* symtab_;
  Layout* layout_;
  Task_token* symtab_lock_;
  Task_token* blocker_;
};

} // End namespace gold.

#endif // !defined(GOLD_COMMON_H)
