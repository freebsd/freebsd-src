/*
 *  include/asm-x86_64/bugs.h
 *
 *  Copyright (C) 1994  Linus Torvalds
 *  Copyright (C) 2000  SuSE
 *
 * This is included by init/main.c to check for architecture-dependent bugs.
 *
 * Needs:
 *	void check_bugs(void);
 */

#include <linux/config.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/msr.h>
#include <asm/pda.h>

static inline void check_fpu(void)
{
	extern void __bad_fxsave_alignment(void);
	if (offsetof(struct task_struct, thread.i387.fxsave) & 15)
		__bad_fxsave_alignment();

	/* This should not be here */	
	set_in_cr4(X86_CR4_OSFXSR);
	set_in_cr4(X86_CR4_OSXMMEXCPT);
}

static void __init check_bugs(void)
{
	identify_cpu(&boot_cpu_data);
	check_fpu();
}
