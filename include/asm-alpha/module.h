#ifndef _ASM_ALPHA_MODULE_H
#define _ASM_ALPHA_MODULE_H
/*
 * This file contains the alpha architecture specific module code.
 */

#define module_map(x)		vmalloc(x)
#define module_unmap(x)		vfree(x)
#define module_arch_init(x)	alpha_module_init(x)
#define arch_init_modules(x)	alpha_init_modules(x)

static inline int
alpha_module_init(struct module *mod)
{
        if (!mod_bound(mod->gp - 0x8000, 0, mod)) {
                printk(KERN_ERR "module_arch_init: mod->gp out of bounds.\n");
                return 1;
        }
	return 0;
}

static inline void
alpha_init_modules(struct module *mod)
{
	__asm__("stq $29,%0" : "=m" (mod->gp));
}

#endif /* _ASM_ALPHA_MODULE_H */
