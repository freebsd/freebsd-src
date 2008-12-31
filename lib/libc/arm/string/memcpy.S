/*	$NetBSD: memcpy.S,v 1.4 2003/10/14 07:51:45 scw Exp $	*/

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/arm/string/memcpy.S,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $");
#if !defined(__XSCALE__) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
