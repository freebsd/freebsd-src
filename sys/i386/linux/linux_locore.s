/* $FreeBSD: src/sys/i386/linux/linux_locore.s,v 1.5.2.1 2000/07/07 00:38:50 obrien Exp $ */

#include "linux_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <i386/linux/linux_syscall.h>		/* system call numbers */

NON_GPROF_ENTRY(linux_sigcode)
	call	*LINUX_SIGF_HANDLER(%esp)
	leal	LINUX_SIGF_SC(%esp),%ebx	/* linux scp */
	movl	LINUX_SC_GS(%ebx),%gs
	push	%eax				/* fake ret addr */
	movl	$LINUX_SYS_linux_sigreturn,%eax	/* linux_sigreturn() */
	int	$0x80				/* enter kernel with args */
0:	jmp	0b
	ALIGN_TEXT
_linux_esigcode:

	.data
	.globl	_linux_szsigcode
_linux_szsigcode:
	.long	_linux_esigcode-_linux_sigcode

	.text
