/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/amd64/gen/infinity.c,v 1.10.30.1 2008/11/25 02:59:29 kensmith Exp $");

#include <math.h>

/* bytes for +Infinity on a 387 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };

/* bytes for NaN */
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
