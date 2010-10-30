// target-select.cc -- select a target for an object file

#include "gold.h"

#include "elfcpp.h"
#include "target-select.h"

namespace
{

// The start of the list of target selectors.

gold::Target_selector* target_selectors;

} // End anonymous namespace.

namespace gold
{

// Construct a Target_selector, which means adding it to the linked
// list.  This runs at global constructor time, so we want it to be
// fast.

Target_selector::Target_selector(int machine, int size, bool big_endian)
  : machine_(machine), size_(size), big_endian_(big_endian)
{
  this->next_ = target_selectors;
  target_selectors = this;
}

// Find the target for an ELF file.

extern Target*
select_target(int machine, int size, bool big_endian, int osabi,
	      int abiversion)
{
  for (Target_selector* p = target_selectors; p != NULL; p = p->next())
    {
      int pmach = p->machine();
      if ((pmach == machine || pmach == elfcpp::EM_NONE)
	  && p->size() == size
	  && p->big_endian() ? big_endian : !big_endian)
	{
	  Target* ret = p->recognize(machine, osabi, abiversion);
	  if (ret != NULL)
	    return ret;
	}
    }
  return NULL;
}

} // End namespace gold.
