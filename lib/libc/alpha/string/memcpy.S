/*	$NetBSD: memcpy.S,v 1.1 1995/08/13 00:40:47 cgd Exp $	*/


#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/alpha/string/memcpy.S,v 1.2.24.1 2008/10/02 02:57:24 kensmith Exp $");

#define MEMCOPY
#include "bcopy.S"
