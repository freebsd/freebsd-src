#include "svr4_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <compat/svr4/svr4_syscall.h>		/* system call numbers */

/* $FreeBSD$ */
	
NON_GPROF_ENTRY(svr4_sigcode)
	call	*SVR4_SIGF_HANDLER(%esp)
	leal	SVR4_SIGF_UC(%esp),%eax	/* ucp (the call may have clobbered the
					   copy at SIGF_UCP(%esp)) */
#ifdef VM86
#warning "VM86 doesn't work yet - do you really want this?"
	testl	$PSL_VM,SVR4_UC_EFLAGS(%eax)
	jnz	1f
#endif
	movl	SVR4_UC_GS(%eax),%gs
1:	pushl	%eax			# pointer to ucontext
	pushl	$1			# set context
	movl	$svr4_sys_context,%eax
	int	$0x80	 		# enter kernel with args on stack
0:	jmp	0b

	ALIGN_TEXT
svr4_esigcode:

	.data
	.globl	svr4_szsigcode
svr4_szsigcode:
	.long	svr4_esigcode - svr4_sigcode

	.text
