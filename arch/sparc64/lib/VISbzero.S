/* $Id: VISbzero.S,v 1.11 2001/03/15 08:51:24 anton Exp $
 * VISbzero.S: High speed clear operations utilizing the UltraSparc
 *        Visual Instruction Set.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996, 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include "VIS.h"

#ifdef __KERNEL__
#include <asm/visasm.h>

#define EXN(x,y,a,b,z) 				\
98: 	x,y;					\
	.section .fixup;			\
	.align	4;				\
99:	ba	VISbzerofixup_ret##z;		\
	 a, b, %o0;				\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, 99b;			\
	.text;					\
	.align	4;
#define EXC(x,y,a,b,c...) 			\
98: 	x,y;					\
	.section .fixup;			\
	.align	4;				\
99:	c;					\
	ba	VISbzerofixup_ret0;		\
	 a, b, %o0;				\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, 99b;			\
	.text;					\
	.align	4;
#define EXO1(x,y) 				\
98: 	x,y;					\
	.section __ex_table;			\
	.align	4;				\
	.word	98b, VISbzerofixup_reto1;	\
	.text;					\
	.align	4;
#define EX(x,y,a,b) EXN(x,y,a,b,0)
#define EX1(x,y,a,b) EXN(x,y,a,b,1)
#define EX2(x,y,a,b) EXN(x,y,a,b,2)
#define EXT(start,end,handler) 			\
	.section __ex_table;			\
	.align	4;				\
	.word	start, 0, end, handler;		\
	.text;					\
	.align	4
#else
#define EX(x,y,a,b)		x,y
#define EX1(x,y,a,b)		x,y
#define EX2(x,y,a,b)		x,y
#define EXC(x,y,a,b,c...)	x,y
#define EXO1(x,y)		x,y
#define EXT(a,b,c)
#endif

#define ZERO_BLOCKS(base, offset, source)			\
	STX	source, [base - offset - 0x38] ASINORMAL;	\
	STX	source, [base - offset - 0x30] ASINORMAL;	\
	STX	source, [base - offset - 0x28] ASINORMAL;	\
	STX	source, [base - offset - 0x20] ASINORMAL;	\
	STX	source, [base - offset - 0x18] ASINORMAL;	\
	STX	source, [base - offset - 0x10] ASINORMAL;	\
	STX	source, [base - offset - 0x08] ASINORMAL;	\
	STX	source, [base - offset - 0x00] ASINORMAL;

#ifdef __KERNEL__
#define RETL	clr %o0
#else
#define RETL	mov %g3, %o0
#endif

	/* Well, bzero is a lot easier to get right than bcopy... */
#ifdef __KERNEL__
	.section	__ex_table,#alloc
	.section	.fixup,#alloc,#execinstr
#endif
	.text
	.align		32
#ifdef __KERNEL__
	.globl		__bzero_begin
__bzero_begin:
	.globl		__bzero, __bzero_noasi
__bzero_noasi:
	rd		%asi, %g5
	ba,pt		%xcc, __bzero+12
	 mov		%g5, %o4
__bzero:
	rd		%asi, %g5
	wr		%g0, ASI_P, %asi		! LSU	Group
	mov		ASI_P, %o4
#else
	.globl		bzero
bzero_private:
bzero:
#ifndef REGS_64BIT
	srl		%o1, 0, %o1
#endif
	mov		%o0, %g3
#endif
	cmp		%o1, 7
	bleu,pn		%xcc, 17f
	 andcc		%o0, 3, %o2
	be,a,pt		%xcc, 4f
	 andcc		%o0, 4, %g0
	cmp		%o2, 3
	be,pn		%xcc, 2f
	 EXO1(STB	%g0, [%o0 + 0x00] ASINORMAL)
	cmp		%o2, 2
	be,pt		%xcc, 2f
	 EX(STB		%g0, [%o0 + 0x01] ASINORMAL, sub %o1, 1)
	EX(STB		%g0, [%o0 + 0x02] ASINORMAL, sub %o1, 2)
2:	sub		%o2, 4, %o2
	sub		%o0, %o2, %o0
	add		%o1, %o2, %o1
	andcc		%o0, 4, %g0
4:	be,pt		%xcc, 2f
	 cmp		%o1, 128
	EXO1(STW	%g0, [%o0] ASINORMAL)
	sub		%o1, 4, %o1
	add		%o0, 4, %o0
2:	blu,pn		%xcc, 9f
	 andcc		%o0, 0x38, %o2
	be,pn		%icc, 6f
	 mov		64, %o5
	andcc		%o0, 8, %g0
	be,pn		%icc, 1f
	 sub		%o5, %o2, %o5
	EX(STX		%g0, [%o0] ASINORMAL, sub %o1, 0)
	add		%o0, 8, %o0
1:	andcc		%o5, 16, %g0
	be,pn		%icc, 1f
	 sub		%o1, %o5, %o1
	EX1(STX		%g0, [%o0] ASINORMAL, add %g0, 0)
	EX1(STX		%g0, [%o0 + 8] ASINORMAL, sub %g0, 8)
	add		%o0, 16, %o0
1:	andcc		%o5, 32, %g0
	be,pn		%icc, 7f
	 andncc		%o1, 0x3f, %o3
	EX(STX		%g0, [%o0] ASINORMAL, add %o1, 32)
	EX(STX		%g0, [%o0 + 8] ASINORMAL, add %o1, 24)
	EX(STX		%g0, [%o0 + 16] ASINORMAL, add %o1, 16)
	EX(STX		%g0, [%o0 + 24] ASINORMAL, add %o1, 8)
	add		%o0, 32, %o0
6:	andncc		%o1, 0x3f, %o3
7:	be,pn		%xcc, 9f
#ifdef __KERNEL__
	 or		%o4, ASI_BLK_OR, %g7 
	wr		%g7, %g0, %asi
	VISEntryHalf
#else
	 wr		%g0, ASI_BLK_P, %asi
#endif
	membar		#StoreLoad | #StoreStore | #LoadStore
	fzero		%f0
	andcc		%o3, 0xc0, %o2
	and		%o1, 0x3f, %o1
	fzero		%f2
	andn		%o3, 0xff, %o3
	faddd		%f0, %f2, %f4
	fmuld		%f0, %f2, %f6
	cmp		%o2, 64
	faddd		%f0, %f2, %f8
	fmuld		%f0, %f2, %f10
	faddd		%f0, %f2, %f12
	brz,pn		%o2, 10f
	 fmuld		%f0, %f2, %f14
	be,pn		%icc, 2f
	 EXC(STBLK	%f0, [%o0 + 0x00] ASIBLK, add %o3, %o2, add %o2, %o1, %o2)
	cmp		%o2, 128
	be,pn		%icc, 2f
	 EXC(STBLK	%f0, [%o0 + 0x40] ASIBLK, add %o3, %o2, add %o2, %o1, %o2; sub %o2, 64, %o2)
	EXC(STBLK	%f0, [%o0 + 0x80] ASIBLK, add %o3, %o2, add %o2, %o1, %o2; sub %o2, 128, %o2)
2:	brz,pn		%o3, 12f
	 add		%o0, %o2, %o0
10:	EX(STBLK	%f0, [%o0 + 0x00] ASIBLK, add %o3, %o1)
	EXC(STBLK	%f0, [%o0 + 0x40] ASIBLK, add %o3, %o1, sub %o1, 64, %o1)
	EXC(STBLK	%f0, [%o0 + 0x80] ASIBLK, add %o3, %o1, sub %o1, 128, %o1)
	EXC(STBLK	%f0, [%o0 + 0xc0] ASIBLK, add %o3, %o1, sub %o1, 192, %o1)
11:	subcc		%o3, 256, %o3
	bne,pt		%xcc, 10b
	 add		%o0, 256, %o0
12:
#ifdef __KERNEL__
	VISExitHalf
	wr		%o4, 0x0, %asi
#else
#ifndef REGS_64BIT
	wr		%g0, FPRS_FEF, %fprs
#endif
#endif
	membar		#StoreLoad | #StoreStore
9:	andcc		%o1, 0xf8, %o2
	be,pn		%xcc, 13f
	 andcc		%o1, 7, %o1
#ifdef __KERNEL__
14:	sethi		%hi(13f), %o4
	srl		%o2, 1, %o3
	sub		%o4, %o3, %o4
	jmpl		%o4 + %lo(13f), %g0
	 add		%o0, %o2, %o0
#else
14:	rd		%pc, %o4
	srl		%o2, 1, %o3
	sub		%o4, %o3, %o4
	jmpl		%o4 + (13f - 14b), %g0
	 add		%o0, %o2, %o0
#endif
12:	ZERO_BLOCKS(%o0, 0xc8, %g0)
	ZERO_BLOCKS(%o0, 0x88, %g0)
	ZERO_BLOCKS(%o0, 0x48, %g0)
	ZERO_BLOCKS(%o0, 0x08, %g0)
	EXT(12b,13f,VISbzerofixup_zb)
13:	be,pn		%xcc, 8f
	 andcc		%o1, 4, %g0
	be,pn		%xcc, 1f
	 andcc		%o1, 2, %g0
	EX(STW		%g0, [%o0] ASINORMAL, and %o1, 7)
	add		%o0, 4, %o0
1:	be,pn		%xcc, 1f
	 andcc		%o1, 1, %g0
	EX(STH		%g0, [%o0] ASINORMAL, and %o1, 3)
	add		%o0, 2, %o0
1:	bne,a,pn	%xcc, 8f
	 EX(STB		%g0, [%o0] ASINORMAL, add %g0, 1)
8:
#ifdef __KERNEL__
	wr		%g5, %g0, %asi
#endif
	retl
	 RETL
17:	be,pn		%xcc, 13b
	 orcc		%o1, 0, %g0
	be,pn		%xcc, 0f
8:	 add		%o0, 1, %o0
	subcc		%o1, 1, %o1
	bne,pt		%xcc, 8b
	 EX(STB		%g0, [%o0 - 1] ASINORMAL, add %o1, 1)
0:
#ifdef __KERNEL__
	wr		%g5, %g0, %asi
#endif
	retl
	 RETL

#ifdef __KERNEL__
	.section	.fixup
	.align		4
VISbzerofixup_reto1:
	mov		%o1, %o0
VISbzerofixup_ret0:
	wr		%g5, %g0, %asi
	retl
	 wr		%g0, 0, %fprs
VISbzerofixup_ret1:
	and		%o5, 0x30, %o5
	add		%o5, %o1, %o5
	ba,pt		%xcc, VISbzerofixup_ret0
	 add		%o0, %o5, %o0
VISbzerofixup_ret2:
	and		%o5, 0x20, %o5
	add		%o5, %o1, %o5
	ba,pt		%xcc, VISbzerofixup_ret0
	 add		%o0, %o5, %o0
VISbzerofixup_zb:
	andcc		%o1, 7, %o1
	sll		%g2, 3, %g2
	add		%o1, 256, %o1
	ba,pt		%xcc, VISbzerofixup_ret0
	 sub		%o1, %g2, %o0
#endif
	.globl		__bzero_end
__bzero_end:
