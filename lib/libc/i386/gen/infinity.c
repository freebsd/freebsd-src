/*
 * infinity.c
 * $FreeBSD: src/lib/libc/i386/gen/infinity.c,v 1.4.2.1 1999/08/29 14:46:36 peter Exp $
 */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
