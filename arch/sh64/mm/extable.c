/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/mm/extable.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 * Completely coherent with i386/sh/...
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/uaccess.h>

extern const struct exception_table_entry __start___ex_table[];
extern const struct exception_table_entry __stop___ex_table[];

static inline unsigned long
search_one_table(const struct exception_table_entry *first,
		 const struct exception_table_entry *last,
		 unsigned long value)
{
        while (first <= last) {
		const struct exception_table_entry *mid;
		long diff;

		mid = (last - first) / 2 + first;
		diff = mid->insn - value;
                if (diff == 0)
                        return mid->fixup;
                else if (diff < 0)

			/*
			 * The dicotomic search assumes the same
			 * upward assignement of linker addresses
			 * for both .text and __ex_table sections.
			 * __ex_table entries gets automagically
			 * ordered.
			 */
                        first = mid+1;
                else
                        last = mid-1;
        }
        return 0;
}


/* Some functions that may trap due to a bad user-mode address have too many loads
   and stores in them to make it at all practical to label each one and put them all in
   the main exception table.

   In particular, the fast memcpy routine is like this.  It's fix-up is just to fall back
   to a slow byte-at-a-time copy, which is handled the conventional way.  So it's functionally
   OK to just handle any trap occurring in the fast memcpy with that fixup. */
unsigned long
check_exception_ranges(unsigned long addr)
{
	extern unsigned long copy_user_memcpy, copy_user_memcpy_end, __copy_user_fixup;
	if ((addr >= (unsigned long) &copy_user_memcpy) && (addr <= (unsigned long) &copy_user_memcpy_end)) {
		return (unsigned long) &__copy_user_fixup;
	}

	return 0; /* no match */
}


unsigned long
search_exception_table(unsigned long addr)
{
	unsigned long ret;

	ret = check_exception_ranges(addr);
	if (ret) return ret;

#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	ret = search_one_table(__start___ex_table, __stop___ex_table-1, addr);
	if (ret) return ret;
#else
	/* The kernel is the last "module" -- no need to treat it special.  */
	struct module *mp;
	for (mp = module_list; mp != NULL; mp = mp->next) {
		if (mp->ex_table_start == NULL)
			continue;
		ret = search_one_table(mp->ex_table_start,
				       mp->ex_table_end - 1, addr);
		if (ret) return ret;
	}
#endif

	return 0;
}
