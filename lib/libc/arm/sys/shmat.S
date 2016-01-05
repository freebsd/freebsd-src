/*	$NetBSD: shmat.S,v 1.1 2000/12/29 20:14:04 bjh21 Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD$");
#include "SYS.h"

RSYSCALL(shmat)

	.section .note.GNU-stack,"",%progbits
