/*	$NetBSD: memcpy.S,v 1.1 1995/08/13 00:40:47 cgd Exp $	*/


#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/alpha/string/memcpy.S,v 1.2 2003/01/19 00:17:03 obrien Exp $");

#define MEMCOPY
#include "bcopy.S"
