/*
 * infinity.c
 *	infinity.c,v 1.2 1995/01/23 01:26:57 davidg Exp
 */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
