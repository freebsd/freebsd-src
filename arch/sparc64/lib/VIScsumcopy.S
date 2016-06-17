/* $Id: VIScsumcopy.S,v 1.8 2000/02/20 23:21:39 davem Exp $
 * VIScsumcopy.S: High bandwidth IP checksumming with simultaneous
 *            copying utilizing the UltraSparc Visual Instruction Set.
 *
 * Copyright (C) 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 *
 * Based on older sparc32/sparc64 checksum.S, which is:
 *
 *      Copyright(C) 1995 Linus Torvalds
 *      Copyright(C) 1995 Miguel de Icaza
 *      Copyright(C) 1996,1997 David S. Miller
 *    derived from:
 *	  Linux/Alpha checksum c-code
 *        Linux/ix86 inline checksum assembly
 *        RFC1071 Computing the Internet Checksum (esp. Jacobsons m68k code)
 *	  David Mosberger-Tang for optimized reference c-code
 *	  BSD4.4 portable checksum routine
 */

#ifdef __sparc_v9__
#define STACKOFF	0x7ff+128
#else
#define STACKOFF	64
#endif

#ifdef __KERNEL__
#include <asm/head.h>
#include <asm/asi.h>
#include <asm/page.h>
#include <asm/visasm.h>
#define ASI_BLK_XOR	0
#define ASI_BLK_XOR1	(ASI_BLK_P ^ (ASI_BLK_P >> 3) ^ ASI_P)
#define ASI_BLK_OR	(ASI_BLK_P & ~ASI_P)
#else
#define ASI_P		0x80
#define ASI_BLK_P	0xf0
#define FRPS_FEF	0x04
#define FPRS_DU		0x02
#define FPRS_DL		0x01
#define ASI_BLK_XOR	(ASI_BLK_P ^ ASI_P)
#endif

#define src		o0
#define dst		o1
#define	len		o2
#define sum		o3
#define x1		g1
#define x2		g2
#define x3		o4
#define x4		g4
#define x5		g5
#define x6		g7
#define x7		g3
#define x8		o5

/* Dobrou noc, SunSoft engineers. Spete sladce.
 * This has a couple of tricks in and those
 * tricks are UltraLinux trade secrets :))
 * Once AGAIN, the SunSoft engineers are caught
 * asleep at the keyboard :)).
 * The main loop does about 20 superscalar cycles
 * per 64bytes checksummed/copied.
 */

#define LDBLK(O0)									\
	ldda		[%src] %asi, %O0 	/*  Load	Group		*/

#define STBLK										\
	stda		%f48, [%dst] ASI_BLK_P	/*  Store			*/

#define ST(fx,off)									\
	std		%fx, [%dst + off]	/*  Store			*/

#define SYNC										\
	membar		#Sync


#define DO_THE_TRICK(f0,f2,f4,f6,f8,f10,f12,f14,F0,F2,F4,F6,F8,F10,F12,F14,DUMMY1,A0,A2,A4,A6,A8,A10,A12,A14,B14,DUMMY2,LOAD,STORE1,STORE2,STORE3,STORE4,STORE5,STORE6,STORE7,STORE8,DUMMY3,BRANCH...)	\
	LOAD					/*  Load	(Group)		*/;	\
	faligndata	%A14, %F0, %A14		/*  FPA		Group		*/;	\
	inc		%x5			/*  IEU0			*/;	\
	STORE1					/*  Store (optional)		*/;	\
	faligndata	%F0, %F2, %A0		/*  FPA		Group		*/;	\
	srl		%x5, 1, %x5		/*  IEU0			*/;	\
	add		%sum, %x4, %sum		/*  IEU1			*/;	\
	fpadd32		%F0, %f0, %F0		/*  FPA		Group		*/;	\
	inc		%x6			/*  IEU0			*/;	\
	STORE2					/*  Store (optional)		*/;	\
	faligndata	%F2, %F4, %A2		/*  FPA		Group		*/;	\
	srl		%x6, 1, %x6		/*  IEU0			*/;	\
	add		%sum, %x5, %sum		/*  IEU1			*/;	\
	fpadd32		%F2, %f2, %F2		/*  FPA		Group		*/;	\
	add		%src, 64, %src		/*  IEU0			*/;	\
	fcmpgt32	%f0, %F0, %x1		/*  FPM				*/;	\
	add		%dst, 64, %dst		/*  IEU1	Group		*/;	\
	inc		%x7			/*  IEU0			*/;	\
	STORE3					/*  Store (optional)		*/;	\
	faligndata	%F4, %F6, %A4		/*  FPA				*/;	\
	fpadd32		%F4, %f4, %F4		/*  FPA		Group		*/;	\
	add		%sum, %x6, %sum		/*  IEU1			*/;	\
	fcmpgt32	%f2, %F2, %x2		/*  FPM				*/;	\
	srl		%x7, 1, %x7		/*  IEU0	Group		*/;	\
	inc		%x8			/*  IEU1			*/;	\
	STORE4					/*  Store (optional)		*/;	\
	faligndata	%F6, %F8, %A6		/*  FPA				*/;	\
	fpadd32		%F6, %f6, %F6		/*  FPA		Group		*/;	\
	srl		%x8, 1, %x8		/*  IEU0			*/;	\
	fcmpgt32	%f4, %F4, %x3		/*  FPM				*/;	\
	add		%sum, %x7, %sum		/*  IEU0	Group		*/;	\
	inc		%x1			/*  IEU1			*/;	\
	STORE5					/*  Store (optional)		*/;	\
	faligndata	%F8, %F10, %A8		/*  FPA				*/;	\
	fpadd32		%F8, %f8, %F8		/*  FPA		Group		*/;	\
	srl		%x1, 1, %x1		/*  IEU0			*/;	\
	fcmpgt32	%f6, %F6, %x4		/*  FPM				*/;	\
	add		%sum, %x8, %sum		/*  IEU0	Group		*/;	\
	inc		%x2			/*  IEU1			*/;	\
	STORE6					/*  Store (optional)		*/;	\
	faligndata	%F10, %F12, %A10	/*  FPA				*/;	\
	fpadd32		%F10, %f10, %F10	/*  FPA		Group		*/;	\
	srl		%x2, 1, %x2		/*  IEU0			*/;	\
	fcmpgt32	%f8, %F8, %x5		/*  FPM				*/;	\
	add		%sum, %x1, %sum		/*  IEU0	Group		*/;	\
	inc		%x3			/*  IEU1			*/;	\
	STORE7					/*  Store (optional)		*/;	\
	faligndata	%F12, %F14, %A12	/*  FPA				*/;	\
	fpadd32		%F12, %f12, %F12	/*  FPA		Group		*/;	\
	srl		%x3, 1, %x3		/*  IEU0			*/;	\
	fcmpgt32	%f10, %F10, %x6		/*  FPM				*/;	\
	add		%sum, %x2, %sum		/*  IEU0	Group		*/;	\
	inc		%x4			/*  IEU1			*/;	\
	STORE8					/*  Store (optional)		*/;	\
	fmovd		%F14, %B14		/*  FPA				*/;	\
	fpadd32		%F14, %f14, %F14	/*  FPA		Group		*/;	\
	srl		%x4, 1, %x4		/*  IEU0			*/;	\
	fcmpgt32	%f12, %F12, %x7		/*  FPM				*/;	\
	add		%sum, %x3, %sum		/*  IEU0	Group		*/;	\
	subcc		%len, 64, %len		/*  IEU1			*/;	\
	BRANCH					/*  CTI				*/;	\
	fcmpgt32	%f14, %F14, %x8		/*  FPM		Group		*/;

#define END_THE_TRICK(f0,f2,f4,f6,f8,f10,f12,f14,FA,FB,S0,S1,S2,S3,T0,T1,U0,fz) \
	inc		%x5			/*  IEU0	Group		*/;	\
	fpadd32		%f2, %f0, %S0		/*  FPA				*/;	\
	add		%sum, %x4, %sum		/*  IEU1			*/;	\
	srl		%x5, 1, %x5		/*  IEU0	Group		*/;	\
	fpadd32		%f6, %f4, %S1		/*  FPA				*/;	\
	inc		%x6			/*  IEU1			*/;	\
	fpadd32		%f10, %f8, %S2		/*  FPA		Group		*/;	\
	add		%sum, %x5, %sum		/*  IEU0			*/;	\
	fcmpgt32	%f0, %S0, %x1		/*  FPM				*/;	\
	fpadd32		%f14, %f12, %S3		/*  FPA		Group		*/;	\
	srl		%x6, 1, %x6		/*  IEU0			*/;	\
	fcmpgt32	%f4, %S1, %x2		/*  FPM				*/;	\
	add		%sum, %x6, %sum		/*  IEU0	Group		*/;	\
	fzero		%fz			/*  FPA				*/;	\
	fcmpgt32	%f8, %S2, %x3		/*  FPM				*/;	\
	inc		%x7			/*  IEU0	Group		*/;	\
	inc		%x8			/*  IEU1			*/;	\
	srl		%x7, 1, %x7		/*  IEU0	Group		*/;	\
	inc		%x1			/*  IEU1			*/;	\
	fpadd32		%S0, %S1, %T0		/*  FPA				*/;	\
	fpadd32		%S2, %S3, %T1		/*  FPA		Group		*/;	\
	add		%sum, %x7, %sum		/*  IEU0			*/;	\
	fcmpgt32	%f12, %S3, %x4		/*  FPM				*/;	\
	srl		%x8, 1, %x8		/*  IEU0	Group		*/;	\
	inc		%x2			/*  IEU1			*/;	\
	srl		%x1, 1, %x1		/*  IEU0	Group		*/;	\
	add		%sum, %x8, %sum		/*  IEU1			*/;	\
	add		%sum, %x1, %sum		/*  IEU0	Group		*/;	\
	fcmpgt32	%S0, %T0, %x5		/*  FPM				*/;	\
	srl		%x2, 1, %x2		/*  IEU0	Group		*/;	\
	fcmpgt32	%S2, %T1, %x6		/*  FPM				*/;	\
	inc		%x3			/*  IEU0	Group		*/;	\
	add		%sum, %x2, %sum		/*  IEU1			*/;	\
	srl		%x3, 1, %x3		/*  IEU0	Group		*/;	\
	inc		%x4			/*  IEU1			*/;	\
	fpadd32		%T0, %T1, %U0		/*  FPA		Group		*/;	\
	add		%sum, %x3, %sum		/*  IEU0			*/;	\
	fcmpgt32	%fz, %f2, %x7		/*  FPM				*/;	\
	srl		%x4, 1, %x4		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %f6, %x8		/*  FPM				*/;	\
	inc		%x5			/*  IEU0	Group		*/;	\
	add		%sum, %x4, %sum		/*  IEU1			*/;	\
	srl		%x5, 1, %x5		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %f10, %x1		/*  FPM				*/;	\
	inc		%x6			/*  IEU0	Group		*/;	\
	add		%sum, %x5, %sum		/*  IEU1			*/;	\
	fmovd		%FA, %FB		/*  FPA		Group		*/;	\
	fcmpgt32	%fz, %f14, %x2		/*  FPM				*/;	\
	srl		%x6, 1, %x6		/*  IEU0	Group		*/;	\
	ba,pt		%xcc, ett		/*  CTI				*/;	\
	 inc		%x7			/*  IEU1			*/;

#define END_THE_TRICK1(f0,f2,f4,f6,f8,f10,f12,f14,FA,FB) 				\
	END_THE_TRICK(f0,f2,f4,f6,f8,f10,f12,f14,FA,FB,f48,f50,f52,f54,f56,f58,f60,f62)

#define END_THE_TRICK2(S0,S1,S2,S3,T0,T1,U0,U1,V0,fz) 					\
	fpadd32		%U0, %U1, %V0		/*  FPA		Group		*/;	\
	srl		%x7, 1, %x7		/*  IEU0			*/;	\
	add		%sum, %x6, %sum		/*  IEU1			*/;	\
	std		%V0, [%sp + STACKOFF]	/*  Store	Group		*/;	\
	inc		%x8			/*  IEU0			*/;	\
	sub		%sum, %x7, %sum		/*  IEU1			*/;	\
	srl		%x8, 1, %x8		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %S1, %x3		/*  FPM				*/;	\
	inc		%x1			/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %S3, %x4		/*  FPM				*/;	\
	srl		%x1, 1, %x1		/*  IEU0	Group		*/;	\
	sub		%sum, %x8, %sum		/*  IEU1			*/;	\
	ldx		[%sp + STACKOFF], %x8	/*  Load	Group		*/;	\
	inc		%x2			/*  IEU0			*/;	\
	sub		%sum, %x1, %sum		/*  IEU1			*/;	\
	srl		%x2, 1, %x2		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %T1, %x5		/*  FPM				*/;	\
	inc		%x3			/*  IEU0	Group		*/;	\
	fcmpgt32	%T0, %U0, %x6		/*  FPM				*/;	\
	srl		%x3, 1, %x3		/*  IEU0	Group		*/;	\
	sub		%sum, %x2, %sum		/*  IEU1			*/;	\
	inc		%x4			/*  IEU0	Group		*/;	\
	sub		%sum, %x3, %sum		/*  IEU1			*/;	\
	srl		%x4, 1, %x4		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %U1, %x7		/*  FPM				*/;	\
	inc		%x5			/*  IEU0	Group		*/;	\
	fcmpgt32	%U0, %V0, %x1		/*  FPM				*/;	\
	srl		%x5, 1, %x5		/*  IEU0	Group		*/;	\
	sub		%sum, %x4, %sum		/*  IEU1			*/;	\
	sub		%sum, %x5, %sum		/*  IEU0	Group		*/;	\
	fcmpgt32	%fz, %V0, %x2		/*  FPM				*/;	\
	inc		%x6			/*  IEU0	Group		*/;	\
	inc		%x7			/*  IEU1			*/;	\
	srl		%x6, 1, %x6		/*  IEU0	Group		*/;	\
	inc		%x1			/*  IEU1			*/;	\
	srl		%x7, 1, %x7		/*  IEU0	Group		*/;	\
	add		%sum, %x6, %sum		/*  IEU1			*/;	\
	srl		%x1, 1, %x1		/*  IEU0	Group		*/;	\
	sub		%sum, %x7, %sum		/*  IEU1			*/;	\
	inc		%x2			/*  IEU0	Group		*/;	\
	add		%sum, %x1, %sum		/*  IEU1			*/;	\
	srl		%x2, 1, %x2		/*  IEU0	Group		*/;	\
	sub		%sum, %x2, %sum		/*  IEU0	Group		*/;	\
	addcc		%sum, %x8, %sum		/*  IEU1	Group		*/;	\
	bcs,a,pn	%xcc, 33f		/*  CTI				*/;	\
	 add		%sum, 1, %sum		/*  IEU0	(Group)		*/;	\
33:						/*  That's it			*/;

	.text
	.globl		csum_partial_copy_vis
	.align		32
/* %asi should be either ASI_P or ASI_AIUS for csum_partial_copy resp.
 * csum_partial_copy_from_user
 * This assumes that !((%src^%dst)&3) && !((%src|%dst)&1) && %len >= 256
 */
csum_partial_copy_vis:
	andcc		%dst, 7, %g0		/*  IEU1	Group		*/
	be,pt		%icc, 4f		/*  CTI				*/
	 and		%dst, 0x38, %o4		/*  IEU0			*/
	mov		1, %g5			/*  IEU0	Group		*/
	andcc		%dst, 2, %g0		/*  IEU1			*/
	be,pt		%icc, 1f		/*  CTI				*/
	 and		%dst, 4, %g7		/*  IEU0	Group		*/
	lduha		[%src] %asi, %g2	/*  Load			*/
	sub		%len, 2, %len		/*  IEU0	Group		*/
	add		%dst, 2, %dst		/*  IEU1			*/
	andcc		%dst, 4, %g7		/*  IEU1	Group		*/
	sll		%g5, 16, %g5		/*  IEU0			*/
	sth		%g2, [%dst - 2]		/*  Store	Group		*/
	sll		%g2, 16, %g2		/*  IEU0	 		*/
	add		%src, 2, %src		/*  IEU1			*/
	addcc		%g2, %sum, %sum		/*  IEU1	Group		*/
	bcs,a,pn	%icc, 1f		/*  CTI				*/
	 add		%sum, %g5, %sum		/*  IEU0			*/
1:	lduwa		[%src] %asi, %g2	/*  Load			*/
	brz,a,pn	%g7, 4f			/*  CTI+IEU1	Group		*/
	 and		%dst, 0x38, %o4		/*  IEU0			*/
	add		%dst, 4, %dst		/*  IEU0	Group		*/
	sub		%len, 4, %len		/*  IEU1			*/
	addcc		%g2, %sum, %sum		/*  IEU1	Group		*/
	bcs,a,pn	%icc, 1f		/*  CTI				*/
	 add		%sum, 1, %sum		/*  IEU0			*/
1:	and		%dst, 0x38, %o4		/*  IEU0	Group		*/
	stw		%g2, [%dst - 4]		/*  Store			*/
	add		%src, 4, %src		/*  IEU1			*/
4:
#ifdef __KERNEL__
	VISEntry
#endif
	mov		%src, %g7		/*  IEU1	Group		*/
	fzero		%f48			/*  FPA				*/
	alignaddr	%src, %g0, %src		/*  Single	Group		*/
	subcc		%g7, %src, %g7		/*  IEU1	Group		*/
	be,pt		%xcc, 1f		/*  CTI				*/
	 mov		0x40, %g1		/*  IEU0			*/
	lduwa		[%src] %asi, %g2	/*  Load	Group		*/
	subcc		%sum, %g2, %sum		/*  IEU1	Group+load stall*/
	bcs,a,pn	%icc, 1f		/*  CTI				*/
	 sub		%sum, 1, %sum		/*  IEU0			*/
1:	srl		%sum, 0, %sum		/*  IEU0	Group		*/
	clr		%g5			/*  IEU1			*/
	brz,pn		%o4, 3f			/*  CTI+IEU1	Group		*/
	 sub		%g1, %o4, %g1		/*  IEU0			*/
	ldda		[%src] %asi, %f0	/*  Load			*/
	clr		%o4			/*  IEU0	Group		*/
	andcc		%dst, 8, %g0		/*  IEU1			*/
	be,pn		%icc, 1f		/*  CTI				*/
	 ldda		[%src + 8] %asi, %f2	/*  Load	Group		*/
	add		%src, 8, %src		/*  IEU0			*/
	sub		%len, 8, %len		/*  IEU1			*/
	fpadd32		%f0, %f48, %f50		/*  FPA				*/
	addcc		%dst, 8, %dst		/*  IEU1	Group		*/
	faligndata	%f0, %f2, %f16		/*  FPA				*/
	fcmpgt32	%f48, %f50, %o4		/*  FPM		Group		*/
	fmovd		%f2, %f0		/*  FPA		Group		*/
	ldda		[%src + 8] %asi, %f2	/*  Load			*/
	std		%f16, [%dst - 8]	/*  Store			*/
	fmovd		%f50, %f48		/*  FPA				*/
1:	andcc		%g1, 0x10, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 1f		/*  CTI				*/
	 and		%g1, 0x20, %g1		/*  IEU0			*/
	fpadd32		%f0, %f48, %f50		/*  FPA				*/
	ldda		[%src + 16] %asi, %f4	/*  Load	Group		*/
	add		%src, 16, %src		/*  IEU0			*/
	add		%dst, 16, %dst		/*  IEU1			*/
	faligndata	%f0, %f2, %f16		/*  FPA				*/
	fcmpgt32	%f48, %f50, %g5		/*  FPM		Group		*/
	sub		%len, 16, %len		/*  IEU0			*/
	inc		%o4			/*  IEU1			*/
	std		%f16, [%dst - 16]	/*  Store	Group		*/
	fpadd32		%f2, %f50, %f48		/*  FPA				*/
	srl		%o4, 1, %o5		/*  IEU0			*/
	faligndata	%f2, %f4, %f18		/*  FPA		Group		*/
	std		%f18, [%dst - 8]	/*  Store			*/
	fcmpgt32	%f50, %f48, %o4		/*  FPM		Group		*/
	add		%o5, %sum, %sum		/*  IEU0			*/
	ldda		[%src + 8] %asi, %f2	/*  Load			*/
	fmovd		%f4, %f0		/*  FPA				*/
1:	brz,a,pn	%g1, 4f			/*  CTI+IEU1	Group		*/
	 rd		%asi, %g2		/*  LSU		Group + 4 bubbles*/
	inc		%g5			/*  IEU0			*/
	fpadd32		%f0, %f48, %f50		/*  FPA				*/
	ldda		[%src + 16] %asi, %f4	/*  Load	Group		*/
	srl		%g5, 1, %g5		/*  IEU0			*/
	add		%dst, 32, %dst		/*  IEU1			*/
	faligndata	%f0, %f2, %f16		/*  FPA				*/
	fcmpgt32	%f48, %f50, %o5		/*  FPM		Group		*/
	inc		%o4			/*  IEU0			*/
	ldda		[%src + 24] %asi, %f6	/*  Load			*/
	srl		%o4, 1, %o4		/*  IEU0	Group		*/
	add		%g5, %sum, %sum		/*  IEU1			*/
	ldda		[%src + 32] %asi, %f8	/*  Load			*/
	fpadd32		%f2, %f50, %f48		/*  FPA				*/
	faligndata	%f2, %f4, %f18		/*  FPA		Group		*/
	sub		%len, 32, %len		/*  IEU0			*/
	std		%f16, [%dst - 32]	/*  Store			*/
	fcmpgt32	%f50, %f48, %g3		/*  FPM		Group		*/
	inc		%o5			/*  IEU0			*/
	add		%o4, %sum, %sum		/*  IEU1			*/
	fpadd32		%f4, %f48, %f50		/*  FPA				*/
	faligndata	%f4, %f6, %f20		/*  FPA		Group		*/
	srl		%o5, 1, %o5		/*  IEU0			*/
	fcmpgt32	%f48, %f50, %g5		/*  FPM		Group		*/
	add		%o5, %sum, %sum		/*  IEU0			*/
	std		%f18, [%dst - 24]	/*  Store			*/
	fpadd32		%f6, %f50, %f48		/*  FPA				*/
	inc		%g3			/*  IEU0	Group		*/
	std		%f20, [%dst - 16]	/*  Store			*/
	add		%src, 32, %src		/*  IEU1			*/
	faligndata	%f6, %f8, %f22		/*  FPA				*/
	fcmpgt32	%f50, %f48, %o4		/*  FPM		Group		*/
	srl		%g3, 1, %g3		/*  IEU0			*/
	std		%f22, [%dst - 8]	/*  Store			*/	
	add		%g3, %sum, %sum		/*  IEU0	Group		*/
3:	rd		%asi, %g2		/*  LSU		Group + 4 bubbles*/
#ifdef __KERNEL__
4:	sethi		%hi(vis0s), %g7		/*  IEU0	Group		*/
	or		%g2, ASI_BLK_OR, %g2	/*  IEU1			*/
#else
4:	rd		%pc, %g7		/*  LSU		Group + 4 bubbles*/
#endif
	inc		%g5			/*  IEU0	Group		*/
	and		%src, 0x38, %g3		/*  IEU1			*/	
	membar		#StoreLoad		/*  LSU		Group		*/
	srl		%g5, 1, %g5		/*  IEU0			*/
	inc		%o4			/*  IEU1			*/
	sll		%g3, 8, %g3		/*  IEU0	Group		*/
	sub		%len, 0xc0, %len	/*  IEU1			*/
	addcc		%g5, %sum, %sum		/*  IEU1	Group		*/
	srl		%o4, 1, %o4		/*  IEU0			*/
	add		%g7, %g3, %g7		/*  IEU0	Group		*/
	add		%o4, %sum, %sum		/*  IEU1			*/
#ifdef __KERNEL__
	jmpl		%g7 + %lo(vis0s), %g0	/*  CTI+IEU1	Group		*/
#else
	jmpl		%g7 + (vis0s - 4b), %g0	/*  CTI+IEU1	Group		*/
#endif
	 fzero		%f32			/*  FPA				*/

	.align		2048
vis0s:	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	add		%src, 128, %src		/*  IEU0	Group		*/
	ldda		[%src-128] %asi, %f0	/*  Load	Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f48, %f62		/*  FPA		Group	f0 available*/
	faligndata	%f0, %f2, %f48		/*  FPA		Group	f2 available*/
	fcmpgt32	%f32, %f2, %x1		/*  FPM		Group	f4 available*/
	fpadd32		%f0, %f62, %f0		/*  FPA				*/
	fcmpgt32	%f32, %f4, %x2		/*  FPM		Group	f6 available*/
	faligndata	%f2, %f4, %f50		/*  FPA				*/
	fcmpgt32	%f62, %f0, %x3		/*  FPM		Group	f8 available*/
	faligndata	%f4, %f6, %f52		/*  FPA				*/
	fcmpgt32	%f32, %f6, %x4		/*  FPM		Group	f10 available*/
	inc		%x1			/*  IEU0			*/
	faligndata	%f6, %f8, %f54		/*  FPA				*/
	fcmpgt32	%f32, %f8, %x5		/*  FPM		Group	f12 available*/
	srl		%x1, 1, %x1		/*  IEU0			*/
	inc		%x2			/*  IEU1			*/
	faligndata	%f8, %f10, %f56		/*  FPA				*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group	f14 available*/
	srl		%x2, 1, %x2		/*  IEU0			*/
	add		%sum, %x1, %sum		/*  IEU1			*/
	faligndata	%f10, %f12, %f58	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	inc		%x3			/*  IEU0			*/
	add		%sum, %x2, %sum		/*  IEU1			*/
	faligndata	%f12, %f14, %f60	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	srl		%x3, 1, %x3		/*  IEU0			*/
	inc		%x4			/*  IEU1			*/
	fmovd		%f14, %f62		/*  FPA				*/
	srl		%x4, 1, %x4		/*  IEU0	Group		*/
	add		%sum, %x3, %sum		/*  IEU1			*/
vis0:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,
			,f48,f50,f52,f54,f56,f58,f60,f62,f62,
			,LDBLK(f32),	STBLK,,,,,,,,
			,bcs,pn %icc, vis0e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f48,f50,f52,f54,f56,f58,f60,f62,f62,
			,LDBLK(f0),	STBLK,,,,,,,,
			,bcs,pn %icc, vis0e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,
			,f48,f50,f52,f54,f56,f58,f60,f62,f62,
			,LDBLK(f16),	STBLK,,,,,,,,
			,bcc,pt %icc, vis0)
vis0e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,
			,f48,f50,f52,f54,f56,f58,f60,f62,f32,
			,SYNC,		STBLK,ST(f48,64),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),ST(f60,48),
			,add %dst, 56, %dst; add %len, 192 - 8*8, %len; ba,pt %icc, e2)
vis0e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f48,f50,f52,f54,f56,f58,f60,f62,f0,
			,SYNC,		STBLK,ST(f48,64),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),ST(f60,48),
			,add %dst, 56, %dst; add %len, 192 - 8*8, %len; ba,pt %icc, e3)
vis0e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,
			,f48,f50,f52,f54,f56,f58,f60,f62,f16,
			,SYNC,		STBLK,ST(f48,64),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),ST(f60,48),
			,add %dst, 56, %dst; add %len, 192 - 8*8, %len; ba,pt %icc, e1)
	.align		2048
vis1s:	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	add		%src, 128 - 8, %src	/*  IEU0	Group		*/
	ldda		[%src-128] %asi, %f0	/*  Load	Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f0, %f58		/*  FPA		Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	fcmpgt32	%f32, %f2, %x2		/*  FPM		Group		*/
	faligndata	%f2, %f4, %f48		/*  FPA				*/
	fcmpgt32	%f32, %f4, %x3		/*  FPM		Group		*/
	faligndata	%f4, %f6, %f50		/*  FPA				*/
	fcmpgt32	%f32, %f6, %x4		/*  FPM		Group		*/
	faligndata	%f6, %f8, %f52		/*  FPA				*/
	fcmpgt32	%f32, %f8, %x5		/*  FPM		Group		*/
	inc		%x2			/*  IEU1			*/
	faligndata	%f8, %f10, %f54		/*  FPA				*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group		*/
	srl		%x2, 1, %x2		/*  IEU0			*/
	faligndata	%f10, %f12, %f56	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	inc		%x3			/*  IEU0			*/
	add		%sum, %x2, %sum		/*  IEU1			*/
	faligndata	%f12, %f14, %f58	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	srl		%x3, 1, %x3		/*  IEU0			*/
	inc		%x4			/*  IEU1			*/
	fmovd		%f14, %f60		/*  FPA				*/
	srl		%x4, 1, %x4		/*  IEU0	Group		*/
	add		%sum, %x3, %sum		/*  IEU1			*/
vis1:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f62,f48,f50,f52,f54,f56,f58,f60,f60,
			,LDBLK(f32),	,STBLK,,,,,,,
			,bcs,pn %icc, vis1e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f62,f48,f50,f52,f54,f56,f58,f60,f60,
			,LDBLK(f0),	,STBLK,,,,,,,
			,bcs,pn %icc, vis1e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f62,f48,f50,f52,f54,f56,f58,f60,f60,
			,LDBLK(f16),	,STBLK,,,,,,,
			,bcc,pt %icc, vis1)
vis1e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f62,f48,f50,f52,f54,f56,f58,f60,f32,
			,SYNC,		,STBLK,ST(f48,0),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),
			,add %dst, 48, %dst; add %len, 192 - 7*8, %len; ba,pt %icc, e2)
vis1e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f62,f48,f50,f52,f54,f56,f58,f60,f0,
			,SYNC,		,STBLK,ST(f48,0),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),
			,add %dst, 48, %dst; add %len, 192 - 7*8, %len; ba,pt %icc, e3)
vis1e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f62,f48,f50,f52,f54,f56,f58,f60,f16,
			,SYNC,		,STBLK,ST(f48,0),ST(f50,8),ST(f52,16),ST(f54,24),ST(f56,32),ST(f58,40),
			,add %dst, 48, %dst; add %len, 192 - 7*8, %len; ba,pt %icc, e1)
	.align		2048
vis2s:	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	add		%src, 128 - 16, %src	/*  IEU0	Group		*/
	ldda		[%src-128] %asi, %f0	/*  Load	Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f0, %f56		/*  FPA		Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/	
	sub		%dst, 64, %dst		/*  IEU0			*/
	fpsub32		%f2, %f2, %f2		/*  FPA		Group		*/
	fcmpgt32	%f32, %f4, %x3		/*  FPM		Group		*/
	faligndata	%f4, %f6, %f48		/*  FPA				*/
	fcmpgt32	%f32, %f6, %x4		/*  FPM		Group		*/
	faligndata	%f6, %f8, %f50		/*  FPA				*/
	fcmpgt32	%f32, %f8, %x5		/*  FPM		Group		*/
	faligndata	%f8, %f10, %f52		/*  FPA				*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group		*/
	faligndata	%f10, %f12, %f54	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	inc		%x3			/*  IEU0			*/
	faligndata	%f12, %f14, %f56	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	srl		%x3, 1, %x3		/*  IEU0			*/
	inc		%x4			/*  IEU1			*/
	fmovd		%f14, %f58		/*  FPA				*/
	srl		%x4, 1, %x4		/*  IEU0	Group		*/
	add		%sum, %x3, %sum		/*  IEU1			*/
vis2:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f60,f62,f48,f50,f52,f54,f56,f58,f58,
			,LDBLK(f32),	,,STBLK,,,,,,
			,bcs,pn %icc, vis2e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f60,f62,f48,f50,f52,f54,f56,f58,f58,
			,LDBLK(f0),	,,STBLK,,,,,,
			,bcs,pn %icc, vis2e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f60,f62,f48,f50,f52,f54,f56,f58,f58,
			,LDBLK(f16),	,,STBLK,,,,,,
			,bcc,pt %icc, vis2)
vis2e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f60,f62,f48,f50,f52,f54,f56,f58,f32,
			,SYNC,		,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),ST(f56,96),
			,add %dst, 104, %dst; add %len, 192 - 6*8, %len; ba,pt %icc, e2)
vis2e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f60,f62,f48,f50,f52,f54,f56,f58,f0,
			,SYNC,		,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),ST(f56,96),
			,add %dst, 104, %dst; add %len, 192 - 6*8, %len; ba,pt %icc, e3)
vis2e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f60,f62,f48,f50,f52,f54,f56,f58,f16,
			,SYNC,		,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),ST(f56,96),
			,add %dst, 104, %dst; add %len, 192 - 6*8, %len; ba,pt %icc, e1)
	.align		2048
vis3s:	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	add		%src, 128 - 24, %src	/*  IEU0	Group		*/
	ldda		[%src-128] %asi, %f0	/*  Load	Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f0, %f54		/*  FPA		Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	sub		%dst, 64, %dst		/*  IEU0			*/
	fpsub32		%f2, %f2, %f2		/*  FPA		Group		*/
	fpsub32		%f4, %f4, %f4		/*  FPA		Group		*/
	fcmpgt32	%f32, %f6, %x4		/*  FPM		Group		*/
	faligndata	%f6, %f8, %f48		/*  FPA				*/
	fcmpgt32	%f32, %f8, %x5		/*  FPM		Group		*/
	faligndata	%f8, %f10, %f50		/*  FPA				*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group		*/
	faligndata	%f10, %f12, %f52	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	faligndata	%f12, %f14, %f54	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	fmovd		%f14, %f56		/*  FPA				*/
	inc		%x4			/*  IEU0			*/
	srl		%x4, 1, %x4		/*  IEU0	Group		*/
vis3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f58,f60,f62,f48,f50,f52,f54,f56,f56,
			,LDBLK(f32),	,,,STBLK,,,,,
			,bcs,pn %icc, vis3e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f58,f60,f62,f48,f50,f52,f54,f56,f56,
			,LDBLK(f0),	,,,STBLK,,,,,
			,bcs,pn %icc, vis3e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f58,f60,f62,f48,f50,f52,f54,f56,f56,
			,LDBLK(f16),	,,,STBLK,,,,,
			,bcc,pt %icc, vis3)
vis3e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f58,f60,f62,f48,f50,f52,f54,f56,f32,
			,SYNC,		,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),
			,add %dst, 96, %dst; add %len, 192 - 5*8, %len; ba,pt %icc, e2)
vis3e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f58,f60,f62,f48,f50,f52,f54,f56,f0,
			,SYNC,		,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),
			,add %dst, 96, %dst; add %len, 192 - 5*8, %len; ba,pt %icc, e3)
vis3e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f58,f60,f62,f48,f50,f52,f54,f56,f16,
			,SYNC,		,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),ST(f54,88),
			,add %dst, 96, %dst; add %len, 192 - 5*8, %len; ba,pt %icc, e1)
	.align		2048
vis4s:	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	add		%src, 128 - 32, %src	/*  IEU0	Group		*/
	ldda		[%src-128] %asi, %f0	/*  Load	Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f0, %f52		/*  FPA		Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	sub		%dst, 64, %dst		/*  IEU0			*/
	fpsub32		%f2, %f2, %f2		/*  FPA		Group		*/
	fpsub32		%f4, %f4, %f4		/*  FPA		Group		*/
	fpsub32		%f6, %f6, %f6		/*  FPA		Group		*/
	clr		%x4			/*  IEU0			*/
	fcmpgt32	%f32, %f8, %x5		/*  FPM		Group		*/
	faligndata	%f8, %f10, %f48		/*  FPA				*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group		*/
	faligndata	%f10, %f12, %f50	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	faligndata	%f12, %f14, %f52	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	fmovd		%f14, %f54		/*  FPA				*/
vis4:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f56,f58,f60,f62,f48,f50,f52,f54,f54,
			,LDBLK(f32),	,,,,STBLK,,,,
			,bcs,pn %icc, vis4e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f56,f58,f60,f62,f48,f50,f52,f54,f54,
			,LDBLK(f0),	,,,,STBLK,,,,
			,bcs,pn %icc, vis4e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f56,f58,f60,f62,f48,f50,f52,f54,f54,
			,LDBLK(f16),	,,,,STBLK,,,,
			,bcc,pt %icc, vis4)
vis4e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f56,f58,f60,f62,f48,f50,f52,f54,f32,
			,SYNC,		,,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),
			,add %dst, 88, %dst; add %len, 192 - 4*8, %len; ba,pt %icc, e2)
vis4e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f56,f58,f60,f62,f48,f50,f52,f54,f0,
			,SYNC,		,,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),
			,add %dst, 88, %dst; add %len, 192 - 4*8, %len; ba,pt %icc, e3)
vis4e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f56,f58,f60,f62,f48,f50,f52,f54,f16,
			,SYNC,		,,,,STBLK,ST(f48,64),ST(f50,72),ST(f52,80),
			,add %dst, 88, %dst; add %len, 192 - 4*8, %len; ba,pt %icc, e1)
	.align		2048
vis5s:	add		%src, 128 - 40, %src	/*  IEU0	Group		*/
	ldda		[%src-88] %asi, %f10	/*  Load	Group		*/
	ldda		[%src-80] %asi, %f12	/*  Load	Group		*/
	ldda		[%src-72] %asi, %f14	/*  Load	Group		*/
	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f2		/*  FPM				*/
	clr		%x4			/*  IEU0			*/
	faddd		%f32, %f32, %f4		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f6		/*  FPM				*/
	clr		%x5			/*  IEU0			*/
	faddd		%f32, %f32, %f8		/*  FPA		Group		*/
	fcmpgt32	%f32, %f10, %x6		/*  FPM		Group		*/
	sub		%dst, 64, %dst		/*  IEU0			*/
	faligndata	%f10, %f12, %f48	/*  FPA				*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	faligndata	%f12, %f14, %f50	/*  FPA				*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	fmovd		%f14, %f52		/*  FPA				*/
vis5:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f54,f56,f58,f60,f62,f48,f50,f52,f52,
			,LDBLK(f32),	,,,,,STBLK,,,
			,bcs,pn %icc, vis5e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f54,f56,f58,f60,f62,f48,f50,f52,f52,
			,LDBLK(f0),	,,,,,STBLK,,,
			,bcs,pn %icc, vis5e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f54,f56,f58,f60,f62,f48,f50,f52,f52,
			,LDBLK(f16),	,,,,,STBLK,,,
			,bcc,pt %icc, vis5)
vis5e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f54,f56,f58,f60,f62,f48,f50,f52,f32,
			,SYNC,		,,,,,STBLK,ST(f48,64),ST(f50,72),
			,add %dst, 80, %dst; add %len, 192 - 3*8, %len; ba,pt %icc, e2)
vis5e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f54,f56,f58,f60,f62,f48,f50,f52,f0,
			,SYNC,		,,,,,STBLK,ST(f48,64),ST(f50,72),
			,add %dst, 80, %dst; add %len, 192 - 3*8, %len; ba,pt %icc, e3)
vis5e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f54,f56,f58,f60,f62,f48,f50,f52,f16,
			,SYNC,		,,,,,STBLK,ST(f48,64),ST(f50,72),
			,add %dst, 80, %dst; add %len, 192 - 3*8, %len; ba,pt %icc, e1)
	.align		2048
vis6s:	add		%src, 128 - 48, %src	/*  IEU0	Group		*/
	ldda		[%src-80] %asi, %f12	/*  Load	Group		*/
	ldda		[%src-72] %asi, %f14	/*  Load	Group		*/
	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f2		/*  FPM				*/
	clr		%x4			/*  IEU0			*/
	faddd		%f32, %f32, %f4		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f6		/*  FPM				*/
	clr		%x5			/*  IEU0			*/
	faddd		%f32, %f32, %f8		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f10	/*  FPM				*/
	clr		%x6			/*  IEU0			*/
	fcmpgt32	%f32, %f12, %x7		/*  FPM		Group		*/
	sub		%dst, 64, %dst		/*  IEU0			*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	faligndata	%f12, %f14, %f48	/*  FPA				*/
	fmovd		%f14, %f50		/*  FPA		Group		*/
vis6:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f52,f54,f56,f58,f60,f62,f48,f50,f50,
			,LDBLK(f32),	,,,,,,STBLK,,
			,bcs,pn %icc, vis6e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f52,f54,f56,f58,f60,f62,f48,f50,f50,
			,LDBLK(f0),	,,,,,,STBLK,,
			,bcs,pn %icc, vis6e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f52,f54,f56,f58,f60,f62,f48,f50,f50,
			,LDBLK(f16),	,,,,,,STBLK,,
			,bcc,pt %icc, vis6)
vis6e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f52,f54,f56,f58,f60,f62,f48,f50,f32,
			,SYNC,		,,,,,,STBLK,ST(f48,64),
			,add %dst, 72, %dst; add %len, 192 - 2*8, %len; ba,pt %icc, e2)
vis6e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f52,f54,f56,f58,f60,f62,f48,f50,f0,
			,SYNC,		,,,,,,STBLK,ST(f48,64),
			,add %dst, 72, %dst; add %len, 192 - 2*8, %len; ba,pt %icc, e3)
vis6e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f52,f54,f56,f58,f60,f62,f48,f50,f16,
			,SYNC,		,,,,,,STBLK,ST(f48,64),
			,add %dst, 72, %dst; add %len, 192 - 2*8, %len; ba,pt %icc, e1)
	.align		2048
vis7s:	add		%src, 128 - 56, %src	/*  IEU0	Group		*/
	ldda		[%src-72] %asi, %f14	/*  Load	Group		*/
	wr		%g2, ASI_BLK_XOR, %asi	/*  LSU		Group		*/
	ldda		[%src-64] %asi, %f16	/*  Load	Group		*/
	fmovd		%f48, %f0		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f2		/*  FPM				*/
	clr		%x4			/*  IEU0			*/
	faddd		%f32, %f32, %f4		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f6		/*  FPM				*/
	clr		%x5			/*  IEU0			*/
	faddd		%f32, %f32, %f8		/*  FPA		Group		*/
	fmuld		%f32, %f32, %f10	/*  FPM				*/
	clr		%x6			/*  IEU0			*/
	faddd		%f32, %f32, %f12	/*  FPA		Group		*/
	clr		%x7			/*  IEU0			*/
	fcmpgt32	%f32, %f14, %x8		/*  FPM		Group		*/
	sub		%dst, 64, %dst		/*  IEU0			*/
	fmovd		%f14, %f48		/*  FPA				*/
vis7:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f50,f52,f54,f56,f58,f60,f62,f48,f48,
			,LDBLK(f32),	,,,,,,,STBLK,
			,bcs,pn %icc, vis7e1)
	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f50,f52,f54,f56,f58,f60,f62,f48,f48,
			,LDBLK(f0),	,,,,,,,STBLK,
			,bcs,pn %icc, vis7e2)
	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f50,f52,f54,f56,f58,f60,f62,f48,f48,
			,LDBLK(f16),	,,,,,,,STBLK,
			,bcc,pt %icc, vis7)
vis7e3:	DO_THE_TRICK(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,	
			,f50,f52,f54,f56,f58,f60,f62,f48,f32,
			,SYNC,		,,,,,,,STBLK,
			,add %dst, 64, %dst; add %len, 192 - 1*8, %len; ba,pt %icc, e2)
vis7e1:	DO_THE_TRICK(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,
			,f50,f52,f54,f56,f58,f60,f62,f48,f0,
			,SYNC,		,,,,,,,STBLK,
			,add %dst, 64, %dst; add %len, 192 - 1*8, %len; ba,pt %icc, e3)
vis7e2:	DO_THE_TRICK(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f2,f4,f6,f8,f10,f12,f14,	
			,f50,f52,f54,f56,f58,f60,f62,f48,f16,
			,SYNC,		,,,,,,,STBLK,
			,add %dst, 64, %dst; add %len, 192 - 1*8, %len; ba,pt %icc, e1)
e1:	END_THE_TRICK1(	f0,f2,f4,f6,f8,f10,f12,f14,f16,f6)
e2:	END_THE_TRICK1(	f16,f18,f20,f22,f24,f26,f28,f30,f32,f6)
e3:	END_THE_TRICK1(	f32,f34,f36,f38,f40,f42,f44,f46,f0,f6)
ett:	rd		%asi, %x4		/*  LSU		Group+4bubbles	*/
	rd		%gsr, %x3		/*  LSU		Group+4bubbles	*/
#ifdef __KERNEL__
	srl		%x4, 3, %x5		/*  IEU0	Group		*/
	xor		%x4, ASI_BLK_XOR1, %x4	/*  IEU1			*/
	wr		%x4, %x5, %asi		/*  LSU		Group+4bubbles	*/
#else
	wr		%x4, ASI_BLK_XOR, %asi	/*  LSU		Group+4bubbles	*/
#endif
	andcc		%x3, 7, %x3		/*  IEU1	Group		*/
	add		%dst, 8, %dst		/*  IEU0			*/
	bne,pn		%icc, 1f		/*  CTI				*/
	 fzero		%f10			/*  FPA				*/
	brz,a,pn	%len, 2f		/*  CTI+IEU1	Group		*/
	 std		%f6, [%dst - 8]		/*  Store			*/
1:	cmp		%len, 8			/*  IEU1			*/
	blu,pn		%icc, 3f		/*  CTI				*/
	 sub		%src, 64, %src		/*  IEU0	Group		*/
1:	ldda		[%src] %asi, %f2	/*  Load	Group		*/
	fpadd32		%f10, %f2, %f12		/*  FPA		Group+load stall*/
	add		%src, 8, %src		/*  IEU0			*/
	add		%dst, 8, %dst		/*  IEU1			*/
	faligndata	%f6, %f2, %f14		/*  FPA		Group		*/
	fcmpgt32	%f10, %f12, %x5		/*  FPM		Group		*/
	std		%f14, [%dst - 16]	/*  Store			*/
	fmovd		%f2, %f6		/*  FPA				*/
	fmovd		%f12, %f10		/*  FPA		Group		*/
	sub		%len, 8, %len		/*  IEU1			*/
	fzero		%f16			/*  FPA		Group - FPU nop	*/
	fzero		%f18			/*  FPA		Group - FPU nop	*/
	inc		%x5			/*  IEU0			*/
	srl		%x5, 1, %x5		/*  IEU0	Group (regdep)	*/
	cmp		%len, 8			/*  IEU1			*/
	bgeu,pt		%icc, 1b		/*  CTI				*/
	 add		%x5, %sum, %sum		/*  IEU0	Group		*/
3:	brz,a,pt	%x3, 2f			/*  CTI+IEU1			*/
	 std		%f6, [%dst - 8]		/*  Store	Group		*/
	st		%f7, [%dst - 8]		/*  Store	Group		*/
	sub		%dst, 4, %dst		/*  IEU0			*/
	add		%len, 4, %len		/*  IEU1			*/
2:
#ifdef __KERNEL__
	sub		%sp, 8, %sp		/*  IEU0	Group		*/
#endif
	END_THE_TRICK2(	f48,f50,f52,f54,f56,f58,f60,f10,f12,f62)
	membar		#Sync			/*  LSU		Group		*/
#ifdef __KERNEL__
	VISExit
	add		%sp, 8, %sp		/*  IEU0	Group		*/
#endif
23:	brnz,pn		%len, 26f		/*  CTI+IEU1	Group		*/
24:	 sllx		%sum, 32, %g1		/*  IEU0			*/
25:	addcc		%sum, %g1, %src		/*  IEU1	Group		*/
	srlx		%src, 32, %src		/*  IEU0	Group (regdep)	*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%src, 1, %src		/*  IEU1			*/
#ifndef __KERNEL__
1:	retl					/*  CTI		Group brk forced*/
	 srl		%src, 0, %src		/*  IEU0			*/
#else
1:	sethi		%uhi(PAGE_OFFSET), %g4	/*  IEU0	Group		*/
	retl					/*  CTI		Group brk forced*/
	 sllx		%g4, 32, %g4		/*  IEU0			*/
#endif
26:	andcc		%len, 8, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 1f		/*  CTI				*/
	 lduwa		[%src] %asi, %o4	/*  Load			*/
	lduwa		[%src+4] %asi, %g2	/*  Load	Group		*/
	add		%src, 8, %src		/*  IEU0			*/
	add		%dst, 8, %dst		/*  IEU1			*/
	sllx		%o4, 32, %g5		/*  IEU0	Group		*/
	stw		%o4, [%dst - 8]		/*  Store			*/
	or		%g5, %g2, %g5		/*  IEU0	Group		*/
	stw		%g2, [%dst - 4]		/*  Store			*/
	addcc		%g5, %sum, %sum		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%sum, 1, %sum		/*  IEU0			*/
1:	andcc		%len, 4, %g0		/*  IEU1	Group		*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%g2			/*  IEU0			*/
	lduwa		[%src] %asi, %g7	/*  Load			*/
	add		%src, 4, %src		/*  IEU0	Group		*/
	add		%dst, 4, %dst		/*  IEU1			*/
	sllx		%g7, 32, %g2		/*  IEU0	Group		*/
	stw		%g7, [%dst - 4]		/*  Store			*/
1:	andcc		%len, 2, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%g3			/*  IEU0	Group		*/
	lduha		[%src] %asi, %g7	/*  Load			*/
	add		%src, 2, %src		/*  IEU1			*/
	add		%dst, 2, %dst		/*  IEU0	Group		*/
	sll		%g7, 16, %g3		/*  IEU0	Group		*/
	sth		%g7, [%dst - 2]		/*  Store			*/
1:	andcc		%len, 1, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%o5			/*  IEU0	Group		*/
	lduba		[%src] %asi, %g7	/*  Load			*/
	sll		%g7, 8, %o5		/*  IEU0	Group		*/
	stb		%g7, [%dst]		/*  Store			*/
1:	or		%g2, %g3, %g3		/*  IEU1			*/
	or		%o5, %g3, %g3		/*  IEU0	Group (regdep)	*/
	addcc		%g3, %sum, %sum		/*  IEU1	Group (regdep)	*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%sum, 1, %sum		/*  IEU0			*/
1:	ba,pt		%xcc, 25b		/*  CTI		Group		*/
	 sllx		%sum, 32, %g1		/*  IEU0			*/

#ifdef __KERNEL__
end:

	.section	__ex_table
	.align		4
	.word		csum_partial_copy_vis, 0, end, cpc_handler
#endif
