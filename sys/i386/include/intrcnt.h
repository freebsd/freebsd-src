/* $FreeBSD$ */

#include <i386/isa/icu.h>

#define	INTRCNT_COUNT (1 + ICU_LEN + 2 * ICU_LEN)

#ifdef _KERNEL
#ifndef LOCORE
extern u_long intrcnt[];	/* counts for for each device and stray */
extern char intrnames[];	/* string table containing device names */
extern char eintrnames[];	/* end of intrnames[] */
#endif
#endif
