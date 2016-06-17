#ifndef _ASM_SPARC64_MODULE_H
#define _ASM_SPARC64_MODULE_H
/*
 * This file contains the sparc64 architecture specific module code.
 */

extern void * module_map (unsigned long size);
extern void module_unmap (void *addr);
#define module_arch_init(x)	(0)
#define arch_init_modules(x)	do { } while (0)

#endif /* _ASM_SPARC64_MODULE_H */
