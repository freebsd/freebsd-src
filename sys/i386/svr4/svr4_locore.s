#include "svr4_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <svr4/svr4_syscall.h>		/* system call numbers */

/* $Id: svr4_locore.s,v 1.6 1999/07/30 12:47:17 newton Exp $ */
	
NON_GPROF_ENTRY(svr4_sigcode)
	call	SVR4_SIGF_HANDLER(%esp)
	leal	SVR4_SIGF_UC(%esp),%eax	# ucp (the call may have clobbered the
					# copy at SIGF_UCP(%esp))
#if defined(NOTYET)
#ifdef VM86
	testl	$PSL_VM,SVR4_UC_EFLAGS(%eax)
	jnz	1f
#endif
#endif
	movl	SVR4_UC_GS(%eax),%edx
	movl	%dx,%gs
1:	pushl	%eax			# fake return address
	pushl	$1			# pointer to ucontext
	movl	$_svr4_sys_context,%eax
	int	$0x80	 		# enter kernel with args on stack
	movl	$exit,%eax
	int	$0x80			# exit if sigreturn fails

	ALIGN_TEXT
svr4_esigcode:

	.data
	.globl	_svr4_szsigcode
_svr4_szsigcode:
	.long	svr4_esigcode - _svr4_sigcode

	.text

