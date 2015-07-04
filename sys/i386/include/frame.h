/*-
 * This file is in the public domain.
 */
/* $FreeBSD$ */

#include <x86/frame.h>

#ifndef __I386_FRAME_H__
#define	__i386_FRAME_H__

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

#endif
