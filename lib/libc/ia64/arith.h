/*
 * MD header for contrib/gdtoa
 *
 * $FreeBSD$
 */

#include <machine/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN

#define IEEE_8087
#define Arith_Kind_ASL 1
#define Long int
#define Intcast (int)(long)
#define Double_Align
#define X64_bit_pointers

#else /* _BYTE_ORDER == _LITTLE_ENDIAN */

#define IEEE_MC68k
#define Arith_Kind_ASL 2
#define Long int
#define Intcast (int)(long)
#define Double_Align
#define X64_bit_pointers
#ifdef gcc_bug	/* XXX Why does arithchk report sudden underflow here? */
#define Sudden_Underflow
#endif

#endif
