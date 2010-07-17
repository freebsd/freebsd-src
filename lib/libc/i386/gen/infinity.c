/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/i386/gen/infinity.c,v 1.10.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#include <math.h>

/* bytes for +Infinity on a 387 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };

/* bytes for NaN */
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
