/* $Id: VISmemset.S,v 1.10 1999/12/23 17:02:16 jj Exp $
 * VISmemset.S: High speed memset operations utilizing the UltraSparc
 *        Visual Instruction Set.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996, 1997, 1999 Jakub Jelinek (jakub@redhat.com)
 */

#include "VIS.h"

#ifdef REGS_64BIT
#define SET_BLOCKS(base, offset, source)	\
	stx	source, [base - offset - 0x18];	\
	stx	source, [base - offset - 0x10];	\
	stx	source, [base - offset - 0x08];	\
	stx	source, [base - offset - 0x00];
#else
#define SET_BLOCKS(base, offset, source)	\
	stw	source, [base - offset - 0x18];	\
	stw	source, [base - offset - 0x14];	\
	stw	source, [base - offset - 0x10];	\
	stw	source, [base - offset - 0x0c];	\
	stw	source, [base - offset - 0x08];	\
	stw	source, [base - offset - 0x04];	\
	stw	source, [base - offset - 0x00];	\
	stw	source, [base - offset + 0x04];
#endif

#ifndef __KERNEL__
/* So that the brz,a,pt in memset doesn't have to get through PLT, here we go... */
#include "VISbzero.S"
#endif

#ifdef __KERNEL__
#include <asm/visasm.h>
#endif

	/* Well, memset is a lot easier to get right than bcopy... */
	.text
	.align		32
#ifdef __KERNEL__
	.globl		__memset
__memset:
#endif
	.globl		memset
memset:
#ifndef __KERNEL__
	brz,a,pt	%o1, bzero_private
	 mov		%o2, %o1
#ifndef REGS_64BIT
	srl		%o2, 0, %o2
#endif
#endif
	mov		%o0, %o4
	cmp		%o2, 7
	bleu,pn		%xcc, 17f
	 andcc		%o0, 3, %g5
	be,pt		%xcc, 4f
	 and		%o1, 0xff, %o1
	cmp		%g5, 3
	be,pn		%xcc, 2f
	 stb		%o1, [%o0 + 0x00]
	cmp		%g5, 2
	be,pt		%xcc, 2f
	 stb		%o1, [%o0 + 0x01]
	stb		%o1, [%o0 + 0x02]
2:	sub		%g5, 4, %g5
	sub		%o0, %g5, %o0
	add		%o2, %g5, %o2
4:	sllx		%o1, 8, %g1
	andcc		%o0, 4, %g0
	or		%o1, %g1, %o1
	sllx		%o1, 16, %g1
	or		%o1, %g1, %o1
	be,pt		%xcc, 2f
#ifdef REGS_64BIT
	 sllx		%o1, 32, %g1
#else
	 cmp		%o2, 128
#endif
	stw		%o1, [%o0]
	sub		%o2, 4, %o2
	add		%o0, 4, %o0
2:
#ifdef REGS_64BIT
	cmp		%o2, 128
	or		%o1, %g1, %o1
#endif
	blu,pn		%xcc, 9f
	 andcc		%o0, 0x38, %g5
	be,pn		%icc, 6f
	 mov		64, %o5
	andcc		%o0, 8, %g0
	be,pn		%icc, 1f
	 sub		%o5, %g5, %o5
#ifdef REGS_64BIT
	stx		%o1, [%o0]
#else
	stw		%o1, [%o0]
	stw		%o1, [%o0 + 4]
#endif
	add		%o0, 8, %o0
1:	andcc		%o5, 16, %g0
	be,pn		%icc, 1f
	 sub		%o2, %o5, %o2
#ifdef REGS_64BIT
	stx		%o1, [%o0]
	stx		%o1, [%o0 + 8]
#else
	stw		%o1, [%o0]
	stw		%o1, [%o0 + 4]
	stw		%o1, [%o0 + 8]
	stw		%o1, [%o0 + 12]
#endif
	add		%o0, 16, %o0
1:	andcc		%o5, 32, %g0
	be,pn		%icc, 7f
	 andncc		%o2, 0x3f, %o3
#ifdef REGS_64BIT
	stx		%o1, [%o0]
	stx		%o1, [%o0 + 8]
	stx		%o1, [%o0 + 16]
	stx		%o1, [%o0 + 24]
#else
	stw		%o1, [%o0]
	stw		%o1, [%o0 + 4]
	stw		%o1, [%o0 + 8]
	stw		%o1, [%o0 + 12]
	stw		%o1, [%o0 + 16]
	stw		%o1, [%o0 + 20]
	stw		%o1, [%o0 + 24]
	stw		%o1, [%o0 + 28]
#endif
	add		%o0, 32, %o0
7:	be,pn		%xcc, 9f
	 nop
#ifdef __KERNEL__
	VISEntryHalf
#endif
	ldd		[%o0 - 8], %f0
18:	rd		%asi, %g2
	wr		%g0, ASI_BLK_P, %asi
	membar		#StoreStore | #LoadStore
	andcc		%o3, 0xc0, %g5
	and		%o2, 0x3f, %o2
	fmovd		%f0, %f2
	fmovd		%f0, %f4
	andn		%o3, 0xff, %o3
	fmovd		%f0, %f6
	cmp		%g5, 64
	fmovd		%f0, %f8
	fmovd		%f0, %f10
	fmovd		%f0, %f12
	brz,pn		%g5, 10f
	 fmovd		%f0, %f14
	be,pn		%icc, 2f
	 stda		%f0, [%o0 + 0x00] %asi
	cmp		%g5, 128
	be,pn		%icc, 2f
	 stda		%f0, [%o0 + 0x40] %asi
	stda		%f0, [%o0 + 0x80] %asi
2:	brz,pn		%o3, 12f
	 add		%o0, %g5, %o0
10:	stda		%f0, [%o0 + 0x00] %asi
	stda		%f0, [%o0 + 0x40] %asi
	stda		%f0, [%o0 + 0x80] %asi
	stda		%f0, [%o0 + 0xc0] %asi
11:	subcc		%o3, 256, %o3
	bne,pt		%xcc, 10b
	 add		%o0, 256, %o0
12:
#ifdef __KERNEL__
	wr		%g2, %g0, %asi
	VISExitHalf
#else
#ifndef REGS_64BIT
	wr		%g0, FPRS_FEF, %fprs
#endif
#endif
	membar		#StoreLoad | #StoreStore
9:	andcc		%o2, 0x78, %g5
	be,pn		%xcc, 13f
	 andcc		%o2, 7, %o2
#ifdef __KERNEL__
14:	srl		%g5, 1, %o3
	sethi		%hi(13f), %g3
	sub		%g3, %o3, %g3
	jmpl		%g3 + %lo(13f), %g0
	 add		%o0, %g5, %o0
#else
14:	rd		%pc, %g3
#ifdef REGS_64BIT
	srl		%g5, 1, %o3
	sub		%g3, %o3, %g3
#else
	sub		%g3, %g5, %g3
#endif
	jmpl		%g3 + (13f - 14b), %g0
	 add		%o0, %g5, %o0
#endif
12:	SET_BLOCKS(%o0, 0x68, %o1)
	SET_BLOCKS(%o0, 0x48, %o1)
	SET_BLOCKS(%o0, 0x28, %o1)
	SET_BLOCKS(%o0, 0x08, %o1)
13:	be,pn		%xcc, 8f
	 andcc		%o2, 4, %g0
	be,pn		%xcc, 1f
	 andcc		%o2, 2, %g0
	stw		%o1, [%o0]
	add		%o0, 4, %o0
1:	be,pn		%xcc, 1f
	 andcc		%o2, 1, %g0
	sth		%o1, [%o0]
	add		%o0, 2, %o0
1:	bne,a,pn	%xcc, 8f
	 stb		%o1, [%o0]
8:	retl
	 mov		%o4, %o0
17:	brz,pn		%o2, 0f
8:	 add		%o0, 1, %o0
	subcc		%o2, 1, %o2
	bne,pt		%xcc, 8b
	 stb		%o1, [%o0 - 1]
0:	retl
	 mov		%o4, %o0
6:
#ifdef REGS_64BIT
	stx		%o1, [%o0]
#else
	stw		%o1, [%o0]
	stw		%o1, [%o0 + 4]
#endif
	andncc		%o2, 0x3f, %o3
	be,pn		%xcc, 9b
	 nop
#ifdef __KERNEL__
	VISEntryHalf
#endif
	ba,pt		%xcc, 18b
	 ldd		[%o0], %f0
