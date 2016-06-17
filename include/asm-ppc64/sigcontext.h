#ifndef _ASM_PPC64_SIGCONTEXT_H
#define _ASM_PPC64_SIGCONTEXT_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/ptrace.h>
#include <asm/elf.h>


struct sigcontext {
	unsigned long	_unused[4];
	int		signal;
	int		_pad0;
	unsigned long	handler;
	unsigned long	oldmask;
	struct pt_regs	*regs;
	elf_gregset_t	gp_regs;
	elf_fpregset_t	fp_regs;
};

#endif /* _ASM_PPC64_SIGCONTEXT_H */
