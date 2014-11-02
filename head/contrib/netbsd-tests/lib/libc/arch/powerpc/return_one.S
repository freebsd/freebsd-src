/*	$NetBSD: return_one.S,v 1.2 2012/03/16 08:51:47 matt Exp $ */

#include <machine/asm.h>

.globl	return_one, return_one_end

_ENTRY(return_one)
	li	%r3, 1
	blr
return_one_end:
END(return_one)
