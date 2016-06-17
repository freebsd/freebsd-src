/*
 * Kernel exception handling table support.  Derived from arch/alpha/mm/extable.c.
 *
 * Copyright (C) 1998, 1999, 2001-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/config.h>

#include <asm/uaccess.h>
#include <asm/module.h>

extern const struct exception_table_entry __start___ex_table[];
extern const struct exception_table_entry __stop___ex_table[];

static inline const struct exception_table_entry *
search_one_table (const struct exception_table_entry *first,
		  const struct exception_table_entry *last,
		  unsigned long ip, unsigned long gp)
{
        while (first <= last) {
		const struct exception_table_entry *mid;
		long diff;

		mid = &first[(last - first)/2];
		diff = (mid->addr + gp) - ip;
                if (diff == 0)
                        return mid;
                else if (diff < 0)
                        first = mid + 1;
                else
                        last = mid - 1;
        }
        return 0;
}

#ifndef CONFIG_MODULES
register unsigned long main_gp __asm__("gp");
#endif

extern spinlock_t modlist_lock;

struct exception_fixup
search_exception_table (unsigned long addr)
{
	const struct exception_table_entry *entry;
	struct exception_fixup fix = { 0 };

#ifndef CONFIG_MODULES
	/* There is only the kernel to search.  */
	entry = search_one_table(__start___ex_table, __stop___ex_table - 1, addr, main_gp);
	if (entry)
		fix.cont = entry->cont + main_gp;
	return fix;
#else
	struct archdata *archdata;
	unsigned long flags;
	struct module *mp;

	/* The kernel is the last "module" -- no need to treat it special. */
	spin_lock_irqsave(&modlist_lock, flags);
	for (mp = module_list; mp; mp = mp->next) {
		if (!mp->ex_table_start)
			continue;
		archdata = (struct archdata *) mp->archdata_start;
		if (!archdata)
			continue;
		entry = search_one_table(mp->ex_table_start, mp->ex_table_end - 1,
					 addr, (unsigned long) archdata->gp);
		if (entry) {
			fix.cont = entry->cont + (unsigned long) archdata->gp;
			break;
		}
	}
	spin_unlock_irqrestore(&modlist_lock, flags);
#endif
	return fix;
}

void
handle_exception (struct pt_regs *regs, struct exception_fixup fix)
{
	regs->r8 = -EFAULT;
	if (fix.cont & 4)
		regs->r9 = 0;
	regs->cr_iip = (long) fix.cont & ~0xf;
	ia64_psr(regs)->ri = fix.cont & 0x3;		/* set continuation slot number */
}
