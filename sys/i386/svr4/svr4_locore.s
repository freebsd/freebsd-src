#include "svr4_assym.h"			/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include <svr4/svr4_syscall.h>		/* system call numbers */

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
#if defined(__NetBSD__)	
	movl	SVR4_UC_FS(%eax),%ecx
	movl	%cx,%fs
1:	pushl	%eax
	pushl	$1			# setcontext(p) == syscontext(1, p) 
	pushl	%eax			# junk to fake return address
	movl	$_svr4_sys_context,%eax
#else
/* must be FreeBSD, right? */
1:	pushl	%eax			# fake return address
	pushl	$1			# pointer to ucontext
	movl	$_svr4_sys_context,%eax
#endif
	int	$0x80	 		# enter kernel with args on stack
	movl	$exit,%eax
	int	$0x80			# exit if sigreturn fails

	.align	2				/* long word align */
svr4_esigcode:

	.data
	.globl	_svr4_szsigcode
_svr4_szsigcode:
	.long	svr4_esigcode - _svr4_sigcode

	.text

