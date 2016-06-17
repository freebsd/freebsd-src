#ifndef _ASM_PPC64_MODULE_H
#define _ASM_PPC64_MODULE_H
/*
 * This file contains the PPC architecture specific module code.
 *
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define module_map(x)		vmalloc(x)
#define module_unmap(x)		vfree(x)
#define arch_init_modules(x)	do { } while (0)
#define module_arch_init(x)  (0)
#endif /* _ASM_PPC64_MODULE_H */
