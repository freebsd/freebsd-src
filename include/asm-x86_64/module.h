#ifndef _ASM_X8664_MODULE_H
#define _ASM_X8664_MODULE_H

/*
 * This file contains the x8664 architecture specific module code.
 * Modules need to be mapped near the kernel code to allow 32bit relocations.
 */

extern void *module_map(unsigned long);
extern void module_unmap(void *);

#define module_arch_init(x)	(0)
#define arch_init_modules(x)	do { } while (0)

#endif 
