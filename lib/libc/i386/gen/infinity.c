/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/i386/gen/infinity.c,v 1.9 2002/10/31 23:05:19 archie Exp $");

#include <math.h>

/* bytes for +Infinity on a 387 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };
