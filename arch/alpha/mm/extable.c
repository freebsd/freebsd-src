/*
 * linux/arch/alpha/mm/extable.c
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/uaccess.h>

extern const struct exception_table_entry __start___ex_table[];
extern const struct exception_table_entry __stop___ex_table[];

static inline unsigned
search_one_table(const struct exception_table_entry *first,
		 const struct exception_table_entry *last,
		 signed long value)
{
	/* Abort early if the search value is out of range.  */
	if (value != (signed int)value)
		return 0;

        while (first <= last) {
		const struct exception_table_entry *mid;
		long diff;

		mid = (last - first) / 2 + first;
		diff = mid->insn - value;
                if (diff == 0)
                        return mid->fixup.unit;
                else if (diff < 0)
                        first = mid+1;
                else
                        last = mid-1;
        }
        return 0;
}

register unsigned long gp __asm__("$29");

static unsigned
search_exception_table_without_gp(unsigned long addr)
{
	unsigned ret;

#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	ret = search_one_table(__start___ex_table, __stop___ex_table - 1,
			       addr - gp);
#else
	extern spinlock_t modlist_lock;
	unsigned long flags;
	/* The kernel is the last "module" -- no need to treat it special. */
	struct module *mp;

	ret = 0;
	spin_lock_irqsave(&modlist_lock, flags);
	for (mp = module_list; mp ; mp = mp->next) {
		if (!mp->ex_table_start || !(mp->flags&(MOD_RUNNING|MOD_INITIALIZING)))
			continue;
		ret = search_one_table(mp->ex_table_start,
				       mp->ex_table_end - 1, addr - mp->gp);
		if (ret)
			break;
	}
	spin_unlock_irqrestore(&modlist_lock, flags);
#endif

	return ret;
}

unsigned
search_exception_table(unsigned long addr, unsigned long exc_gp)
{
	unsigned ret;

#ifndef CONFIG_MODULES
	ret = search_one_table(__start___ex_table, __stop___ex_table - 1,
			       addr - exc_gp);
	if (ret) return ret;
#else
	extern spinlock_t modlist_lock;
	unsigned long flags;
	/* The kernel is the last "module" -- no need to treat it special. */
	struct module *mp;

	ret = 0;
	spin_lock_irqsave(&modlist_lock, flags);
	for (mp = module_list; mp ; mp = mp->next) {
		if (!mp->ex_table_start || !(mp->flags&(MOD_RUNNING|MOD_INITIALIZING)))
			continue;
		ret = search_one_table(mp->ex_table_start,
				       mp->ex_table_end - 1, addr - exc_gp);
		if (ret)
			break;
	}
	spin_unlock_irqrestore(&modlist_lock, flags);
	if (ret) return ret;
#endif

	/*
	 * The search failed with the exception gp. To be safe, try the
	 * old method before giving up.
	 */
	ret = search_exception_table_without_gp(addr);
	if (ret) {
		printk(KERN_ALERT "%s: [%lx] EX_TABLE search fail with"
		       "exc frame GP, success with raw GP\n",
		       current->comm, addr);
		return ret;
	}

	return 0;
}
