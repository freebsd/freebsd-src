/*
 * Copyright (C) 2000 Anton Blanchard (anton@linuxcare.com)
 *
 * This file implements mcount(), which is used to collect profiling data.
 * This can also be tweaked for kernel stack overflow detection.
 */

#include <linux/config.h>
#include <linux/linkage.h>

#include <asm/asm_offsets.h>
#include <asm/ptrace.h>

/*
 * This is the main variant and is called by C code.  GCC's -pg option
 * automatically instruments every C function with a call to this.
 */

#ifdef CONFIG_STACK_DEBUG

#define OVSTACKSIZE	4096		/* lets hope this is enough */

	.data
	.align		8
panicstring:
	.asciz		"Stack overflow\n"
	.align		8
ovstack:
	.skip		OVSTACKSIZE
#endif
	.text
	.align 32
	.globl mcount
mcount:
#ifdef CONFIG_STACK_DEBUG
	/*
	 * Check whether %sp is dangerously low.
	 */
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_fpdepth], %g1
	srl		%g1, 1, %g5
	add		%g5, 1, %g5
	sllx		%g5, 8, %g5			! each fpregs frame is 256b
	add		%g5, 192, %g5
	add		%g6, %g5, %g5			! where does task_struct+frame end?
	sub		%g5, STACK_BIAS, %g5
	cmp		%sp, %g5
	bg,pt		%xcc, 1f
	 sethi		%hi(panicstring), %g5
	sethi		%hi(ovstack), %g7		! cant move to panic stack fast enough
	 or		%g7, %lo(ovstack), %g7
	add		%g7, OVSTACKSIZE, %g7
	sub		%g7, STACK_BIAS, %g7
	mov		%g7, %sp
	call		prom_printf
	 or		%g5, %lo(panicstring), %o0
	call		prom_halt
	 nop
#endif
1:	retl
	 nop
