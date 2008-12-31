/* $FreeBSD: src/sys/amd64/linux32/linux32_locore.s,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $ */

#include "linux32_assym.h"			/* system definitions */
#include <machine/asmacros.h>			/* miscellaneous asm macros */

#include <amd64/linux32/linux32_syscall.h>	/* system call numbers */

.text
.code32

NON_GPROF_ENTRY(linux_sigcode)
	call	*LINUX_SIGF_HANDLER(%esp)
	leal	LINUX_SIGF_SC(%esp),%ebx	/* linux scp */
	movl	LINUX_SC_ES(%ebx),%es
	movl	LINUX_SC_DS(%ebx),%ds
	movl	%esp, %ebx			/* pass sigframe */
	push	%eax				/* fake ret addr */
	movl	$LINUX_SYS_linux_sigreturn,%eax	/* linux_sigreturn() */
	int	$0x80				/* enter kernel with args */
0:	jmp	0b
	ALIGN_TEXT
/* XXXXX */
linux_rt_sigcode:
	call	*LINUX_RT_SIGF_HANDLER(%esp)
	leal	LINUX_RT_SIGF_UC(%esp),%ebx	/* linux ucp */
	leal	LINUX_RT_SIGF_SC(%ebx),%ecx	/* linux sigcontext */
	movl	LINUX_SC_ES(%ecx),%es
	movl	LINUX_SC_DS(%ecx),%ds
	push	%eax				/* fake ret addr */
	movl	$LINUX_SYS_linux_rt_sigreturn,%eax   /* linux_rt_sigreturn() */
	int	$0x80				/* enter kernel with args */
0:	jmp	0b
	ALIGN_TEXT
/* XXXXX */
linux_esigcode:

	.data
	.globl	linux_szsigcode, linux_sznonrtsigcode
linux_szsigcode:
	.long	linux_esigcode-linux_sigcode
linux_sznonrtsigcode:
	.long	linux_rt_sigcode-linux_sigcode
