/*
 * Offsets into structures used from asm.  Must be kept in sync with
 * appropriate headers.
 *
 * $FreeBSD$
 */

#define	UC_MCONTEXT	0x40

#define	MC_FLAGS	0x0
#define	MC_VALID_FLAGS	0x1
#define	MC_GLOBAL	0x0
#define	MC_OUT		0x40
#define	MC_TPC		0xc8
#define	MC_TNPC		0xc0
