/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/amd64/gen/infinity.c,v 1.10.28.1 2008/10/02 02:57:24 kensmith Exp $");

#include <math.h>

/* bytes for +Infinity on a 387 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };

/* bytes for NaN */
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
