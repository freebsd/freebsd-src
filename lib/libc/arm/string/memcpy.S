/*	$NetBSD: memcpy.S,v 1.4 2003/10/14 07:51:45 scw Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/arm/string/memcpy.S,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $");
#if !defined(__XSCALE__) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
