/*	$NetBSD: memmove.S,v 1.1 2005/12/20 19:28:50 christos Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/mips/string/memmove.S,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");

#define MEMMOVE
#include "bcopy.S"
