/*
 * Offsets into structures used from asm.  Must be kept in sync with
 * appropriate headers.
 *
 * $FreeBSD: src/lib/libpthread/arch/sparc64/sparc64/assym.s,v 1.2 2003/10/09 14:48:09 deischen Exp $
 */

#define	UC_MCONTEXT	0x40

#define	MC_FLAGS	0x0
#define	MC_VALID_FLAGS	0x1
#define	MC_GLOBAL	0x0
#define	MC_OUT		0x40
#define	MC_TPC		0xc8
#define	MC_TNPC		0xc0
