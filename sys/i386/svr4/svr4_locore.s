#include "svr4_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <svr4/svr4_syscall.h>		/* system call numbers */

/* $FreeBSD: src/sys/i386/svr4/svr4_locore.s,v 1.10.2.1 2000/07/07 00:38:51 obrien Exp $ */
	
NON_GPROF_ENTRY(svr4_sigcode)
	call	*SVR4_SIGF_HANDLER(%esp)
	leal	SVR4_SIGF_UC(%esp),%eax	# ucp (the call may have clobbered the
					# copy at SIGF_UCP(%esp))
#ifdef VM86
#warning "VM86 doesn't work yet - do you really want this?"
	testl	$PSL_VM,SVR4_UC_EFLAGS(%eax)
	jnz	1f
#endif
	movl	SVR4_UC_GS(%eax),%gs
1:	pushl	%eax			# pointer to ucontext
	pushl	$1			# set context
	movl	$_svr4_sys_context,%eax
	int	$0x80	 		# enter kernel with args on stack
0:	jmp	0b

	ALIGN_TEXT
svr4_esigcode:

	.data
	.globl	_svr4_szsigcode
_svr4_szsigcode:
	.long	svr4_esigcode - _svr4_sigcode

	.text

