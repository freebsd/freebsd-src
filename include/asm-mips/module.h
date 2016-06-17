#ifndef _ASM_MIPS_MODULE_H
#define _ASM_MIPS_MODULE_H
/*
 * This file contains the mips architecture specific module code.
 */

#include <linux/module.h>
#include <asm/uaccess.h>

#define module_map(x)		vmalloc(x)
#define module_unmap(x)		vfree(x)
#define module_arch_init(x)	mips_module_init(x)
#define arch_init_modules(x)	mips_init_modules(x)

/*
 * This must match in size and layout the data created by
 * modutils/obj/obj-mips.c
 */
struct archdata {
	const struct exception_table_entry *dbe_table_start;
	const struct exception_table_entry *dbe_table_end;
};

static inline int
mips_module_init(struct module *mod)
{
	struct archdata *archdata;

	if (!mod_member_present(mod, archdata_end))
		return 0;

	archdata = (struct archdata *)(mod->archdata_start);
	if (!mod_archdata_member_present(mod, struct archdata, dbe_table_end))
		return 0;

	if (archdata->dbe_table_start > archdata->dbe_table_end ||
	    (archdata->dbe_table_start &&
	     !((unsigned long)archdata->dbe_table_start >=
	       ((unsigned long)mod + mod->size_of_struct) &&
	       ((unsigned long)archdata->dbe_table_end <
	        (unsigned long)mod + mod->size))) ||
            (((unsigned long)archdata->dbe_table_start -
	      (unsigned long)archdata->dbe_table_end) %
	     sizeof(struct exception_table_entry))) {
		printk(KERN_ERR
			"module_arch_init: archdata->dbe_table_* invalid.\n");
		return 1;
	}

	return 0;
}

static inline void
mips_init_modules(struct module *mod)
{
	extern const struct exception_table_entry __start___dbe_table[];
	extern const struct exception_table_entry __stop___dbe_table[];
	static struct archdata archdata = {
		.dbe_table_start	= __start___dbe_table,
		.dbe_table_end		= __stop___dbe_table,
	};

	mod->archdata_start = (char *)&archdata;
	mod->archdata_end = mod->archdata_start + sizeof(archdata);
}

#endif /* _ASM_MIPS_MODULE_H */
