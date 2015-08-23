/* $FreeBSD$ */

#include "linux_assym.h"			/* system definitions */
#include <machine/asmacros.h>			/* miscellaneous asm macros */

#include <amd64/linux/linux_syscall.h>		/* system call numbers */

	.data

	.globl linux_platform
linux_platform:
	.asciz "x86_64"


	.text
/*
 * To avoid excess stack frame the signal trampoline code emulates
 * the 'call' instruction.
 */
NON_GPROF_ENTRY(linux_rt_sigcode)
	movq	%rsp, %rbx			/* preserve sigframe */
	call	.getip
.getip:
	popq	%rax
	add	$.startrtsigcode-.getip, %rax	/* ret address */
	pushq	%rax
	jmp	*LINUX_RT_SIGF_HANDLER(%rbx)
.startrtsigcode:
	movq	$LINUX_SYS_linux_rt_sigreturn,%rax   /* linux_rt_sigreturn() */
	syscall					/* enter kernel with args */
	hlt
0:	jmp	0b

NON_GPROF_ENTRY(__vdso_clock_gettime)
	movq	$LINUX_SYS_linux_clock_gettime,%rax
	syscall
	ret
.weak clock_gettime
.set clock_gettime, __vdso_clock_gettime

NON_GPROF_ENTRY(__vdso_time)
	movq	$LINUX_SYS_linux_time,%rax
	syscall
	ret
.weak time
.set time, __vdso_time

NON_GPROF_ENTRY(__vdso_gettimeofday)
	movq	$LINUX_SYS_gettimeofday,%rax
	syscall
	ret
.weak gettimeofday
.set gettimeofday, __vdso_gettimeofday

NON_GPROF_ENTRY(__vdso_getcpu)
	movq	$-38,%rax	/* not implemented */
	ret
.weak getcpu
.set getcpu, __vdso_getcpu

#if 0
	.section .note.Linux, "a",@note
	.long 2f - 1f		/* namesz */
	.balign 4
	.long 4f - 3f		/* descsz */
	.long 0
1:
	.asciz "Linux"
2:
	.balign 4
3:
	.long LINUX_VERSION_CODE
4:
	.balign 4
	.previous
#endif
