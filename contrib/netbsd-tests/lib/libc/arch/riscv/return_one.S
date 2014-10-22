/*	$NetBSD: return_one.S,v 1.1 2014/09/19 17:36:26 matt Exp $ */

#include <machine/asm.h>

	.globl	return_one_end

ENTRY_NP(return_one)
	li	v0, 1
	ret
return_one_end:
END(return_one)
