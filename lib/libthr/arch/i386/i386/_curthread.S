# $FreeBSD: src/lib/libthr/arch/i386/i386/_curthread.S,v 1.1 2003/04/01 03:46:28 jeff Exp $

#include <machine/asm.h>

ENTRY(_get_curthread)
	cmpl	$0, _thread_initial
	je	nothreads
	movl	%gs:0, %eax
	ret
nothreads:
	xor	%eax, %eax
	ret

ENTRY(_set_gs)
	movl	4(%esp), %eax
	movl	%eax, %gs
	ret
