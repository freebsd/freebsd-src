/*
 * infinity.c
 *	$Id: infinity.c,v 1.4 1997/02/22 14:58:37 peter Exp $
 */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
