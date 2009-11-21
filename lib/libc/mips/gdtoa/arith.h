/* $NetBSD: arith.h,v 1.1 2006/01/25 15:33:28 kleink Exp $ */
/* $FreeBSD: src/lib/libc/mips/gdtoa/arith.h,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $ */

#include <machine/endian.h>

#if BYTE_ORDER == BIG_ENDIAN
#define IEEE_BIG_ENDIAN
#else
#define IEEE_LITTLE_ENDIAN
#endif
