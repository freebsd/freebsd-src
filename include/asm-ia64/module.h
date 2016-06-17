#ifndef _ASM_IA64_MODULE_H
#define _ASM_IA64_MODULE_H
/*
 * This file contains the ia64 architecture specific module code.
 *
 * Copyright (C) 2000 Intel Corporation.
 * Copyright (C) 2000 Mike Stephens <mike.stephens@intel.com>
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <asm/unwind.h>

#define module_map(x)		vmalloc(x)
#define module_unmap(x)		ia64_module_unmap(x)
#define module_arch_init(x)	ia64_module_init(x)

/*
 * This must match in size and layout the data created by
 * modutils/obj/obj-ia64.c
 */
struct archdata {
	const char *unw_table;
	const char *segment_base;
	const char *unw_start;
	const char *unw_end;
	const char *gp;
};

static inline void
arch_init_modules (struct module *kmod)
{
	static struct archdata archdata;
	register char *kernel_gp asm ("gp");

	archdata.gp = kernel_gp;
	kmod->archdata_start = (const char *) &archdata;
	kmod->archdata_end   = (const char *) (&archdata + 1);
}

/*
 * functions to add/remove a modules unwind info when
 * it is loaded or unloaded.
 */
static inline int
ia64_module_init (struct module *mod)
{
	struct archdata *archdata;

	if (!mod_member_present(mod, archdata_start) || !mod->archdata_start)
		return 0;
	archdata = (struct archdata *)(mod->archdata_start);

	if (archdata->unw_start == 0)
		return 0;

	/*
	 * Make sure the unwind pointers are sane.
	 */

	if (archdata->unw_table) {
		printk(KERN_ERR "module_arch_init: archdata->unw_table must be zero.\n");
		return 1;
	}
	if (!mod_bound(archdata->gp, 0, mod)) {
		printk(KERN_ERR "module_arch_init: archdata->gp out of bounds.\n");
		return 1;
	}
	if (!mod_bound(archdata->unw_start, 0, mod)) {
		printk(KERN_ERR "module_arch_init: archdata->unw_start out of bounds.\n");
		return 1;
	}
	if (!mod_bound(archdata->unw_end, 0, mod)) {
		printk(KERN_ERR "module_arch_init: archdata->unw_end out of bounds.\n");
		return 1;
	}
	if (!mod_bound(archdata->segment_base, 0, mod)) {
		printk(KERN_ERR "module_arch_init: archdata->segment_base out of bounds.\n");
		return 1;
	}

	/*
	 * Pointers are reasonable, add the module unwind table
	 */
	archdata->unw_table = unw_add_unwind_table(mod->name,
						   (unsigned long) archdata->segment_base,
						   (unsigned long) archdata->gp,
						   archdata->unw_start, archdata->unw_end);
	return 0;
}

static inline void
ia64_module_unmap (void * addr)
{
	struct module *mod = (struct module *) addr;
	struct archdata *archdata;

	/*
	 * Before freeing the module memory remove the unwind table entry
	 */
	if (mod_member_present(mod, archdata_start) && mod->archdata_start) {
		archdata = (struct archdata *)(mod->archdata_start);

		if (archdata->unw_table != NULL)
			unw_remove_unwind_table((void *) archdata->unw_table);
	}

	vfree(addr);
}

#endif /* _ASM_IA64_MODULE_H */
