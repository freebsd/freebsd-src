/* $FreeBSD: src/lib/libthr/arch/i386/i386/_curthread.S,v 1.3 2004/08/15 16:21:30 dfr Exp $ */

#include <machine/asm.h>

ENTRY(_get_curthread)
	cmpl	$0, _thread_initial
	je	nothreads
	movl	%gs:8, %eax
	ret
nothreads:
	xor	%eax, %eax
	ret

ENTRY(_set_gs)
	movl	4(%esp), %eax
	movl	%eax, %gs
	ret
