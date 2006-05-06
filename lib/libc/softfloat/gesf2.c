/* $NetBSD: gesf2.c,v 1.1 2000/06/06 08:15:05 bjh21 Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/softfloat/gesf2.c,v 1.1 2004/05/14 12:13:04 cognet Exp $");

flag __gesf2(float32, float32);

flag
__gesf2(float32 a, float32 b)
{

	/* libgcc1.c says (a >= b) - 1 */
	return float32_le(b, a) - 1;
}
