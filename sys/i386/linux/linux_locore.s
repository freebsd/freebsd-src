#include "linux_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <i386/linux/linux_syscall.h>		/* system call numbers */

NON_GPROF_ENTRY(linux_sigcode)
	call	LINUX_SIGF_HANDLER(%esp)
	leal	LINUX_SIGF_SC(%esp),%ebx	/* linux scp */
	movl	LINUX_SC_GS(%ebx),%edx
	movl	%dx,%gs
	push	%eax				/* fake ret addr */
	movl	$LINUX_SYS_linux_sigreturn,%eax	/* linux_sigreturn() */
	int	$0x80				/* enter kernel with args on stack */
	hlt					/* never gets here */

	.align	2				/* long word align */
_linux_esigcode:

	.data
	.globl	_linux_szsigcode
_linux_szsigcode:
	.long	_linux_esigcode-_linux_sigcode

	.text
