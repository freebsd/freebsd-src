/*
 * infinity.c
 * $FreeBSD: src/lib/libc/i386/gen/infinity.c,v 1.5 1999/08/27 23:59:21 peter Exp $
 */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
