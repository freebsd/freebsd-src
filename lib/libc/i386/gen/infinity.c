/*
 * infinity.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <math.h>

/* bytes for +Infinity on a 387 */
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };
