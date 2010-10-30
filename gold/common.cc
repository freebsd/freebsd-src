// common.cc -- handle common symbols for gold

#include "gold.h"

#include <algorithm>

#include "workqueue.h"
#include "layout.h"
#include "output.h"
#include "symtab.h"
#include "common.h"

namespace gold
{

// Allocate_commons_task methods.

// This task allocates the common symbols.  We need a lock on the
// symbol table.

Task::Is_runnable_type
Allocate_commons_task::is_runnable(Workqueue*)
{
  if (!this->symtab_lock_->is_writable())
    return IS_LOCKED;
  return IS_RUNNABLE;
}

// Return the locks we hold: one on the symbol table, and one blocker.

class Allocate_commons_task::Allocate_commons_locker : public Task_locker
{
 public:
  Allocate_commons_locker(Task_token& symtab_lock, Task* task,
			  Task_token& blocker, Workqueue* workqueue)
    : symtab_locker_(symtab_lock, task),
      blocker_(blocker, workqueue)
  { }

 private:
  Task_locker_write symtab_locker_;
  Task_locker_block blocker_;
};

Task_locker*
Allocate_commons_task::locks(Workqueue* workqueue)
{
  return new Allocate_commons_locker(*this->symtab_lock_, this,
				     *this->blocker_, workqueue);
}

// Allocate the common symbols.

void
Allocate_commons_task::run(Workqueue*)
{
  this->symtab_->allocate_commons(this->options_, this->layout_);
}

// This class is used to sort the common symbol by size.  We put the
// larger common symbols first.

template<int size>
class Sort_commons
{
 public:
  Sort_commons(const Symbol_table* symtab)
    : symtab_(symtab)
  { }

  bool operator()(const Symbol* a, const Symbol* b) const;

 private:
  const Symbol_table* symtab_;
};

template<int size>
bool
Sort_commons<size>::operator()(const Symbol* pa, const Symbol* pb) const
{
  if (pa == NULL)
    return false;
  if (pb == NULL)
    return true;

  const Symbol_table* symtab = this->symtab_;
  const Sized_symbol<size>* psa;
  psa = symtab->get_sized_symbol SELECT_SIZE_NAME(size) (pa
                                                         SELECT_SIZE(size));
  const Sized_symbol<size>* psb;
  psb = symtab->get_sized_symbol SELECT_SIZE_NAME(size) (pb
                                                         SELECT_SIZE(size));

  typename Sized_symbol<size>::Size_type sa = psa->symsize();
  typename Sized_symbol<size>::Size_type sb = psb->symsize();
  if (sa < sb)
    return false;
  else if (sb > sa)
    return true;

  // When the symbols are the same size, we sort them by alignment.
  typename Sized_symbol<size>::Value_type va = psa->value();
  typename Sized_symbol<size>::Value_type vb = psb->value();
  if (va < vb)
    return false;
  else if (vb > va)
    return true;

  // Otherwise we stabilize the sort by sorting by name.
  return strcmp(psa->name(), psb->name()) < 0;
}

// Allocate the common symbols.

void
Symbol_table::allocate_commons(const General_options& options, Layout* layout)
{
  if (this->get_size() == 32)
    this->do_allocate_commons<32>(options, layout);
  else if (this->get_size() == 64)
    this->do_allocate_commons<64>(options, layout);
  else
    gold_unreachable();
}

// Allocated the common symbols, sized version.

template<int size>
void
Symbol_table::do_allocate_commons(const General_options&,
				  Layout* layout)
{
  typedef typename Sized_symbol<size>::Value_type Value_type;
  typedef typename Sized_symbol<size>::Size_type Size_type;

  // We've kept a list of all the common symbols.  But the symbol may
  // have been resolved to a defined symbol by now.  And it may be a
  // forwarder.  First remove all non-common symbols.
  bool any = false;
  uint64_t addralign = 0;
  for (Commons_type::iterator p = this->commons_.begin();
       p != this->commons_.end();
       ++p)
    {
      Symbol* sym = *p;
      if (sym->is_forwarder())
	{
	  sym = this->resolve_forwards(sym);
	  *p = sym;
	}
      if (!sym->is_common())
	*p = NULL;
      else
	{
	  any = true;
	  Sized_symbol<size>* ssym;
	  ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (
              sym
              SELECT_SIZE(size));
	  if (ssym->value() > addralign)
	    addralign = ssym->value();
	}
    }
  if (!any)
    return;

  // Sort the common symbols by size, so that they pack better into
  // memory.
  std::sort(this->commons_.begin(), this->commons_.end(),
	    Sort_commons<size>(this));

  // Place them in a newly allocated .bss section.

  Output_data_space *poc = new Output_data_space(addralign);

  layout->add_output_section_data(".bss", elfcpp::SHT_NOBITS,
				  elfcpp::SHF_WRITE | elfcpp::SHF_ALLOC,
				  poc);

  // Allocate them all.

  off_t off = 0;
  for (Commons_type::iterator p = this->commons_.begin();
       p != this->commons_.end();
       ++p)
    {
      Symbol* sym = *p;
      if (sym == NULL)
	break;

      Sized_symbol<size>* ssym;
      ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (sym
                                                            SELECT_SIZE(size));

      off = align_address(off, ssym->value());

      Size_type symsize = ssym->symsize();
      ssym->init(ssym->name(), poc, off, symsize, ssym->type(),
		 ssym->binding(), ssym->visibility(), ssym->nonvis(),
		 false);

      off += symsize;
    }

  poc->set_space_size(off);

  this->commons_.clear();
}

} // End namespace gold.
