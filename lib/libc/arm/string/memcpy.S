/*	$NetBSD: memcpy.S,v 1.4 2003/10/14 07:51:45 scw Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/arm/string/memcpy.S,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $");
#if !defined(_ARM_ARCH_5E) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
