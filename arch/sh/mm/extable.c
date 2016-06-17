/* $Id: extable.c,v 1.2 2001/03/31 10:43:18 gniibe Exp $
 *
 * linux/arch/sh/mm/extable.c
 *  Taken from:
 *   linux/arch/i386/mm/extable.c
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
                        first = mid+1;
                else
                        last = mid-1;
        }
        return 0;
}

unsigned long
search_exception_table(unsigned long addr)
{
	unsigned long ret;

#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	ret = search_one_table(__start___ex_table, __stop___ex_table-1, addr);
	if (ret) return ret;
#else
	/* The kernel is the last "module" -- no need to treat it special.  */
	struct module *mp;
	for (mp = module_list; mp != NULL; mp = mp->next) {
		if (mp->ex_table_start == NULL || !(mp->flags&(MOD_RUNNING|MOD_INITIALIZING)))
			continue;
		ret = search_one_table(mp->ex_table_start,
				       mp->ex_table_end - 1, addr);
		if (ret) return ret;
	}
#endif

	return 0;
}
