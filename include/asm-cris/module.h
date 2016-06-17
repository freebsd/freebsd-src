#ifndef _ASM_CRIS_MODULE_H
#define _ASM_CRIS_MODULE_H
/*
 * This file contains the CRIS architecture specific module code.
 */

#define module_map(x)		vmalloc(x)
#define module_unmap(x)		vfree(x)
#define module_arch_init(x)	(0)
#define arch_init_modules(x)    do { } while (0)

#endif /* _ASM_CRIS_MODULE_H */
