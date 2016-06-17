/*
 * Kernel exception handling table support.  Derived from arch/i386/mm/extable.c.
 *
 * Copyright (C) 2000 Hewlett-Packard Co
 * Copyright (C) 2000 John Marvin (jsm@fc.hp.com)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <asm/uaccess.h>


extern const struct exception_table_entry __start___ex_table[];
extern const struct exception_table_entry __stop___ex_table[];

static inline const struct exception_table_entry *
search_one_table (const struct exception_table_entry *first,
		  const struct exception_table_entry *last,
		  unsigned long addr)
{
	/* Abort early if the search value is out of range.  */

	if ((addr < first->addr) || (addr > last->addr))
		return 0;

        while (first <= last) {
		const struct exception_table_entry *mid;
		long diff;

		mid = first + ((last - first)/2);
		diff = mid->addr - addr;

                if (diff == 0)
                        return mid;
                else if (diff < 0)
                        first = mid+1;
                else
                        last = mid-1;
        }

        return 0;
}

const struct exception_table_entry *
search_exception_table (unsigned long addr)
{
#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	return search_one_table(__start___ex_table, 
                                __stop___ex_table - 1, 
                                addr);
#else
	/* The kernel is the last "module" -- no need to treat it special. */
	struct module *mp;

	for (mp = module_list; mp ; mp = mp->next) {
		const struct exception_table_entry *ret;
		if (!mp->ex_table_start)
			continue;
		ret = search_one_table(mp->ex_table_start, mp->ex_table_end - 1,
				       addr);
		if (ret)
			return ret;
	}
	return 0;
#endif
}
