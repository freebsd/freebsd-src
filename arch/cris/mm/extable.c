/*
 * linux/arch/cris/mm/extable.c
 *
 * $Log: extable.c,v $
 * Revision 1.4  2002/03/19 15:21:10  bjornw
 * Fixed warning
 *
 * Revision 1.3  2001/09/27 13:52:40  bjornw
 * Harmonize underscore-ness with other parts
 *
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
                        first = mid+1;
                else
                        last = mid-1;
        }
        return 0;
}

unsigned long
search_exception_table(unsigned long addr)
{
#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	return search_one_table(__start___ex_table, __stop___ex_table-1, addr);
#else
	/* The kernel is the last "module" -- no need to treat it special.  */
	unsigned long ret;
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
