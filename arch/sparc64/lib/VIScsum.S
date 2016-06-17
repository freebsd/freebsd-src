/* $Id: VIScsum.S,v 1.6 2000/02/20 23:21:39 davem Exp $
 * VIScsum.S: High bandwidth IP checksumming utilizing the UltraSparc
 *            Visual Instruction Set.
 *
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 *
 * Based on older sparc32/sparc64 checksum.S, which is:
 *
 *      Copyright(C) 1995 Linus Torvalds
 *      Copyright(C) 1995 Miguel de Icaza
 *      Copyright(C) 1996, 1997 David S. Miller
 *    derived from:
 *	  Linux/Alpha checksum c-code
 *        Linux/ix86 inline checksum assembly
 *        RFC1071 Computing the Internet Checksum (esp. Jacobsons m68k code)
 *	  David Mosberger-Tang for optimized reference c-code
 *	  BSD4.4 portable checksum routine
 */

#ifdef __sparc_v9__
#define STACKOFF	2175
#else
#define STACKOFF	64
#endif

#ifdef __KERNEL__
#include <asm/head.h>
#include <asm/asi.h>
#include <asm/visasm.h>
#include <asm/asm_offsets.h>
#else
#define ASI_BLK_P	0xf0
#define FRPS_FEF	0x04
#endif

/* Dobrou noc, SunSoft engineers. Spete sladce.
 * This has a couple of tricks in and those
 * tricks are UltraLinux trade secrets :))
 */

#define START_THE_TRICK(fz,f0,f2,f4,f6,f8,f10)						\
	fcmpgt32	%fz, %f0, %g1		/*  FPM		Group	*/;		\
	fcmpgt32	%fz, %f2, %g2		/*  FPM		Group	*/;		\
	fcmpgt32	%fz, %f4, %g3		/*  FPM		Group	*/;		\
	inc		%g1			/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f6, %g5		/*  FPM			*/;		\
	srl		%g1, 1, %g1		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f8, %g7		/*  FPM			*/;		\
	inc		%g2			/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f10, %o3		/*  FPM			*/;		\
	srl		%g2, 1, %g2		/*  IEU0	Group	*/;		\
	inc		%g3			/*  IEU1		*/;		\
	srl		%g3, 1, %g3		/*  IEU0	Group	*/;		\
	add		%o2, %g1, %o2		/*  IEU1		*/;		\
	add		%o2, %g2, %o2		/*  IEU0	Group	*/;		\
	inc		%g5			/*  IEU1		*/;		\
	add		%o2, %g3, %o2		/*  IEU0	Group	*/;

#define DO_THE_TRICK(O12,O14,f0,f2,f4,f6,f8,f10,f12,f14,F0,F2,F4,F6,F8,F10,F12,F14)	\
	srl		%g5, 1, %g5		/*  IEU0	Group	*/;		\
	fpadd32		%F0, %f0, %F0		/*  FPA			*/;		\
	fcmpgt32	%O12, %f12, %o4		/*  FPM			*/;		\
	inc		%g7			/*  IEU0	Group	*/;		\
	fpadd32		%F2, %f2, %F2		/*  FPA			*/;		\
	fcmpgt32	%O14, %f14, %o5		/*  FPM			*/;		\
	add		%o2, %g5, %o2		/*  IEU1	Group	*/;		\
	fpadd32		%F4, %f4, %F4		/*  FPA			*/;		\
	fcmpgt32	%f0, %F0, %g1		/*  FPM			*/;		\
	srl		%g7, 1, %g7		/*  IEU0	Group	*/;		\
	fpadd32		%F6, %f6, %F6		/*  FPA			*/;		\
	fcmpgt32	%f2, %F2, %g2		/*  FPM			*/;		\
	add		%o2, %g7, %o2		/*  IEU0	Group	*/;		\
	fpadd32		%F8, %f8, %F8		/*  FPA			*/;		\
	fcmpgt32	%f4, %F4, %g3		/*  FPM			*/;		\
	inc		%o3			/*  IEU0	Group	*/;		\
	fpadd32		%F10, %f10, %F10	/*  FPA			*/;		\
	fcmpgt32	%f6, %F6, %g5		/*  FPM			*/;		\
	srl		%o3, 1, %o3		/*  IEU0	Group	*/;		\
	fpadd32		%F12, %f12, %F12	/*  FPA			*/;		\
	fcmpgt32	%f8, %F8, %g7		/*  FPM			*/;		\
	add		%o2, %o3, %o2		/*  IEU0	Group	*/;		\
	fpadd32		%F14, %f14, %F14	/*  FPA			*/;		\
	fcmpgt32	%f10, %F10, %o3		/*  FPM			*/;		\
	inc		%o4			/*  IEU0	Group	*/;		\
	inc		%o5			/*  IEU1		*/;		\
	srl		%o4, 1, %o4		/*  IEU0	Group	*/;		\
	inc		%g1			/*  IEU1		*/;		\
	srl		%o5, 1, %o5		/*  IEU0	Group	*/;		\
	add		%o2, %o4, %o2		/*  IEU1		*/;		\
	srl		%g1, 1, %g1		/*  IEU0	Group	*/;		\
	add		%o2, %o5, %o2		/*  IEU1		*/;		\
	inc		%g2			/*  IEU0	Group	*/;		\
	add		%o2, %g1, %o2		/*  IEU1		*/;		\
	srl		%g2, 1, %g2		/*  IEU0	Group	*/;		\
	inc		%g3			/*  IEU1		*/;		\
	srl		%g3, 1, %g3		/*  IEU0	Group	*/;		\
	add		%o2, %g2, %o2		/*  IEU1		*/;		\
	inc		%g5			/*  IEU0	Group	*/;		\
	add		%o2, %g3, %o2		/*  IEU0		*/;

#define END_THE_TRICK(O12,O14,f0,f2,f4,f6,f8,f10,f12,f14,S0,S1,S2,S3,T0,T1,U0,fz)	\
	srl		%g5, 1, %g5		/*  IEU0	Group	*/;		\
	fpadd32		%f2, %f0, %S0		/*  FPA			*/;		\
	fcmpgt32	%O12, %f12, %o4		/*  FPM			*/;		\
	inc		%g7			/*  IEU0	Group	*/;		\
	fpadd32		%f6, %f4, %S1		/*  FPA			*/;		\
	fcmpgt32	%O14, %f14, %o5		/*  FPM			*/;		\
	srl		%g7, 1, %g7		/*  IEU0	Group	*/;		\
	fpadd32		%f10, %f8, %S2		/*  FPA			*/;		\
	fcmpgt32	%f0, %S0, %g1		/*  FPM			*/;		\
	inc		%o3			/*  IEU0	Group	*/;		\
	fpadd32		%f14, %f12, %S3		/*  FPA			*/;		\
	fcmpgt32	%f4, %S1, %g2		/*  FPM			*/;		\
	add		%o2, %g5, %o2		/*  IEU0	Group	*/;		\
	fpadd32		%S0, %S1, %T0		/*  FPA			*/;		\
	fcmpgt32	%f8, %S2, %g3		/*  FPM			*/;		\
	add		%o2, %g7, %o2		/*  IEU0	Group	*/;		\
	fzero		%fz			/*  FPA			*/;		\
	fcmpgt32	%f12, %S3, %g5		/*  FPM			*/;		\
	srl		%o3, 1, %o3		/*  IEU0	Group	*/;		\
	fpadd32		%S2, %S3, %T1		/*  FPA			*/;		\
	fcmpgt32	%S0, %T0, %g7		/*  FPM			*/;		\
	add		%o2, %o3, %o2		/*  IEU0	Group	*/;		\
	fpadd32		%T0, %T1, %U0		/*  FPA			*/;		\
	fcmpgt32	%S2, %T1, %o3		/*  FPM			*/;		\
	inc		%o4			/*  IEU0	Group	*/;		\
	inc		%o5			/*  IEU1		*/;		\
	srl		%o4, 1, %o4		/*  IEU0	Group	*/;		\
	inc		%g1			/*  IEU1		*/;		\
	add		%o2, %o4, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f2, %o4		/*  FPM			*/;		\
	srl		%o5, 1, %o5		/*  IEU0	Group	*/;		\
	inc		%g2			/*  IEU1		*/;		\
	add		%o2, %o5, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f6, %o5		/*  FPM			*/;		\
	srl		%g1, 1, %g1		/*  IEU0	Group	*/;		\
	inc		%g3			/*  IEU1		*/;		\
	add		%o2, %g1, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f10, %g1		/*  FPM			*/;		\
	srl		%g2, 1, %g2		/*  IEU0	Group	*/;		\
	inc		%g5			/*  IEU1		*/;		\
	add		%o2, %g2, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %f14, %g2		/*  FPM			*/;		\
	srl		%g3, 1, %g3		/*  IEU0	Group	*/;		\
	inc		%g7			/*  IEU1		*/;		\
	add		%o2, %g3, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %S1, %g3		/*  FPM			*/;		\
	srl		%g5, 1, %g5		/*  IEU0	Group	*/;		\
	inc		%o3			/*  IEU1		*/;		\
	add		%o2, %g5, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %S3, %g5		/*  FPM			*/;		\
	srl		%g7, 1, %g7		/*  IEU0	Group	*/;		\
	inc		%o4			/*  IEU1		*/;		\
	add		%o2, %g7, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %T1, %g7		/*  FPM			*/;		\
	srl		%o3, 1, %o3		/*  IEU0	Group	*/;		\
	inc		%o5			/*  IEU1		*/;		\
	add		%o2, %o3, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%T0, %U0, %o3		/*  FPM			*/;		\
	srl		%o4, 1, %o4		/*  IEU0	Group	*/;		\
	inc		%g1			/*  IEU1		*/;		\
	sub		%o2, %o4, %o2		/*  IEU0	Group	*/;		\
	fcmpgt32	%fz, %U0, %o4		/*  FPM			*/;		\
	srl		%o5, 1, %o5		/*  IEU0	Group	*/;		\
	inc		%g2			/*  IEU1		*/;		\
	srl		%g1, 1, %g1		/*  IEU0	Group	*/;		\
	sub		%o2, %o5, %o2		/*  IEU1		*/;		\
	std		%U0, [%sp + STACKOFF]	/*  Store		*/;		\
	srl		%g2, 1, %g2		/*  IEU0	Group	*/;		\
	sub		%o2, %g1, %o2		/*  IEU1		*/;		\
	inc		%g3			/*  IEU0	Group	*/;		\
	sub		%o2, %g2, %o2		/*  IEU1		*/;		\
	srl		%g3, 1, %g3		/*  IEU0	Group	*/;		\
	inc		%g5			/*  IEU1		*/;		\
	srl		%g5, 1, %g5		/*  IEU0	Group	*/;		\
	sub		%o2, %g3, %o2		/*  IEU1		*/;		\
	ldx		[%sp + STACKOFF], %o5	/*  Load	Group	*/;		\
	inc		%g7			/*  IEU0		*/;		\
	sub		%o2, %g5, %o2		/*  IEU1		*/;		\
	srl		%g7, 1, %g7		/*  IEU0	Group	*/;		\
	inc		%o3			/*  IEU1		*/;		\
	srl		%o3, 1, %o3		/*  IEU0	Group	*/;		\
	sub		%o2, %g7, %o2		/*  IEU1		*/;		\
	inc		%o4			/*  IEU0	Group	*/;		\
	add		%o2, %o3, %o2		/*  IEU1		*/;		\
	srl		%o4, 1, %o4		/*  IEU0	Group	*/;		\
	sub		%o2, %o4, %o2		/*  IEU0	Group	*/;		\
	addcc		%o2, %o5, %o2		/*  IEU1	Group	*/;		\
	bcs,a,pn	%xcc, 33f		/*  CTI			*/;		\
	 add		%o2, 1, %o2		/*  IEU0		*/;		\
33:						/*  That's it		*/;

#define CSUM_LASTCHUNK(offset)								\
        ldx             [%o0 - offset - 0x10], %g2;					\
        ldx             [%o0 - offset - 0x08], %g3;					\
        addcc           %g2, %o2, %o2;							\
        bcs,a,pn        %xcc, 31f;							\
         add            %o2, 1, %o2;							\
31:     addcc           %g3, %o2, %o2;							\
        bcs,a,pn        %xcc, 32f;							\
         add            %o2, 1, %o2;							\
32:

	.text
	.globl		csum_partial
	.align		32
csum_partial:
	andcc		%o0, 7, %g0		/*  IEU1	Group		*/
	be,pt		%icc, 4f		/*  CTI				*/
	 andcc		%o0, 0x38, %g3		/*  IEU1			*/
	mov		1, %g5			/*  IEU0	Group		*/
	cmp		%o1, 6			/*  IEU1			*/
	bl,pn		%icc, 21f		/*  CTI				*/
	 andcc		%o0, 1, %g0		/*  IEU1	Group		*/
	bne,pn		%icc, csump_really_slow /*  CTI				*/
	 andcc		%o0, 2, %g0		/*  IEU1	Group		*/
	be,pt		%icc, 1f		/*  CTI				*/
	 and		%o0, 4, %g7		/*  IEU0			*/
	lduh		[%o0], %g2		/*  Load			*/
	sub		%o1, 2, %o1		/*  IEU0	Group		*/
	add		%o0, 2, %o0		/*  IEU1			*/
	andcc		%o0, 4, %g7		/*  IEU1	Group		*/
	sll		%g5, 16, %g5		/*  IEU0			*/
	sll		%g2, 16, %g2		/*  IEU0	Group 		*/
	addcc		%g2, %o2, %o2		/*  IEU1	Group (regdep)	*/
	bcs,a,pn	%icc, 1f		/*  CTI				*/
	 add		%o2, %g5, %o2		/*  IEU0			*/
1:	ld		[%o0], %g2		/*  Load			*/
	brz,a,pn	%g7, 4f			/*  CTI+IEU1	Group		*/
	 and		%o0, 0x38, %g3		/*  IEU0			*/
	add		%o0, 4, %o0		/*  IEU0	Group		*/
	sub		%o1, 4, %o1		/*  IEU1			*/
	addcc		%g2, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%icc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	and		%o0, 0x38, %g3		/*  IEU1	Group		*/
4:	srl		%o2, 0, %o2		/*  IEU0	Group		*/
	mov		0x40, %g1		/*  IEU1			*/
	brz,pn		%g3, 3f			/*  CTI+IEU1	Group		*/
	 sub		%g1, %g3, %g1		/*  IEU0			*/
	cmp		%o1, 56			/*  IEU1	Group		*/
	blu,pn		%icc, 20f		/*  CTI				*/
	 andcc		%o0, 8, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 1f		/*  CTI				*/
	 ldx		[%o0], %g2		/*  Load			*/
	add		%o0, 8, %o0		/*  IEU0	Group		*/
	sub		%o1, 8, %o1		/*  IEU1			*/
	addcc		%g2, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	andcc		%g1, 0x10, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 2f		/*  CTI				*/
	 and		%g1, 0x20, %g1		/*  IEU0			*/
	ldx		[%o0], %g2		/*  Load			*/
	ldx		[%o0+8], %g3		/*  Load	Group		*/
	add		%o0, 16, %o0		/*  IEU0			*/
	sub		%o1, 16, %o1		/*  IEU1			*/
	addcc		%g2, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	addcc		%g3, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 2f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
2:	brz,pn		%g1, 3f			/*  CTI+IEU1	Group		*/
	 ldx		[%o0], %g2		/*  Load			*/
	ldx		[%o0+8], %g3		/*  Load	Group		*/
	ldx		[%o0+16], %g5		/*  Load	Group		*/
	ldx		[%o0+24], %g7		/*  Load	Group		*/
	add		%o0, 32, %o0		/*  IEU0			*/
	sub		%o1, 32, %o1		/*  IEU1			*/
	addcc		%g2, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	addcc		%g3, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	addcc		%g5, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	addcc		%g7, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 3f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
3:	cmp		%o1, 0xc0		/*  IEU1	Group		*/
	blu,pn		%icc, 20f		/*  CTI				*/
	 sllx		%o2, 32, %g5		/*  IEU0			*/
#ifdef __KERNEL__
	VISEntry
#endif
	addcc		%o2, %g5, %o2		/*  IEU1	Group		*/
	sub		%o1, 0xc0, %o1		/*  IEU0			*/
	wr		%g0, ASI_BLK_P, %asi	/*  LSU		Group		*/
	membar		#StoreLoad		/*  LSU		Group		*/
	srlx		%o2, 32, %o2		/*  IEU0	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU1			*/
1:	andcc		%o1, 0x80, %g0		/*  IEU1	Group		*/
	bne,pn		%icc, 7f		/*  CTI				*/
	 andcc		%o1, 0x40, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 6f		/*  CTI				*/
	 fzero		%f12			/*  FPA				*/
	fzero		%f14			/*  FPA		Group		*/
	ldda		[%o0 + 0x000] %asi, %f16
	ldda		[%o0 + 0x040] %asi, %f32
	ldda		[%o0 + 0x080] %asi, %f48
	START_THE_TRICK(f12,f16,f18,f20,f22,f24,f26)
	ba,a,pt		%xcc, 3f
6:	sub		%o0, 0x40, %o0		/*  IEU0	Group		*/
	fzero		%f28			/*  FPA				*/
	fzero		%f30			/*  FPA		Group		*/
	ldda		[%o0 + 0x040] %asi, %f32
	ldda		[%o0 + 0x080] %asi, %f48
	ldda		[%o0 + 0x0c0] %asi, %f0
	START_THE_TRICK(f28,f32,f34,f36,f38,f40,f42)
	ba,a,pt		%xcc, 4f
7:	bne,pt		%icc, 8f		/*  CTI				*/
	 fzero		%f44			/*  FPA				*/
	add		%o0, 0x40, %o0		/*  IEU0	Group		*/
	fzero		%f60			/*  FPA				*/
	fzero		%f62			/*  FPA		Group		*/
	ldda		[%o0 - 0x040] %asi, %f0
	ldda		[%o0 + 0x000] %asi, %f16
	ldda		[%o0 + 0x040] %asi, %f32
	START_THE_TRICK(f60,f0,f2,f4,f6,f8,f10)
	ba,a,pt		%xcc, 2f
8:	add		%o0, 0x80, %o0		/*  IEU0	Group		*/
	fzero		%f46			/*  FPA				*/
	ldda		[%o0 - 0x080] %asi, %f48
	ldda		[%o0 - 0x040] %asi, %f0
	ldda		[%o0 + 0x000] %asi, %f16
	START_THE_TRICK(f44,f48,f50,f52,f54,f56,f58)
1:	DO_THE_TRICK(f44,f46,f48,f50,f52,f54,f56,f58,f60,f62,f0,f2,f4,f6,f8,f10,f12,f14)
	ldda		[%o0 + 0x040] %asi, %f32
2:	DO_THE_TRICK(f60,f62,f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30)
	ldda		[%o0 + 0x080] %asi, %f48
3:	DO_THE_TRICK(f12,f14,f16,f18,f20,f22,f24,f26,f28,f30,f32,f34,f36,f38,f40,f42,f44,f46)
	ldda		[%o0 + 0x0c0] %asi, %f0
4:	DO_THE_TRICK(f28,f30,f32,f34,f36,f38,f40,f42,f44,f46,f48,f50,f52,f54,f56,f58,f60,f62)
	add		%o0, 0x100, %o0		/*  IEU0	Group		*/
	subcc		%o1, 0x100, %o1		/*  IEU1			*/
	bgeu,a,pt	%icc, 1b		/*  CTI				*/
	 ldda		[%o0 + 0x000] %asi, %f16
	membar		#Sync			/*  LSU		Group		*/
	DO_THE_TRICK(f44,f46,f48,f50,f52,f54,f56,f58,f60,f62,f0,f2,f4,f6,f8,f10,f12,f14)
	END_THE_TRICK(f60,f62,f0,f2,f4,f6,f8,f10,f12,f14,f16,f18,f20,f22,f24,f26,f28,f30)
#ifdef __KERNEL__
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_current_ds], %g7
#endif
	and		%o1, 0x3f, %o1		/*  IEU0	Group		*/
#ifdef __KERNEL__
	VISExit
	wr		%g7, %g0, %asi
#endif
20:	andcc		%o1, 0xf0, %g1		/*  IEU1	Group		*/
	be,pn		%icc, 23f		/*  CTI				*/
	 and		%o1, 0xf, %o3		/*  IEU0			*/
#ifdef __KERNEL__
22:	sll		%g1, 1, %o4		/*  IEU0	Group		*/
	sethi		%hi(23f), %g7		/*  IEU1			*/
	sub		%g7, %o4, %g7		/*  IEU0	Group		*/
	jmpl		%g7 + %lo(23f), %g0	/*  CTI		Group brk forced*/
	 add		%o0, %g1, %o0		/*  IEU0			*/
#else
22:	rd		%pc, %g7		/*  LSU		Group+4bubbles	*/
	sll		%g1, 1, %o4		/*  IEU0	Group		*/
	sub		%g7, %o4, %g7		/*  IEU0	Group (regdep)	*/
	jmpl		%g7 + (23f - 22b), %g0	/*  CTI		Group brk forced*/
	 add		%o0, %g1, %o0		/*  IEU0			*/
#endif
	CSUM_LASTCHUNK(0xe0)
	CSUM_LASTCHUNK(0xd0)
	CSUM_LASTCHUNK(0xc0)
	CSUM_LASTCHUNK(0xb0)
	CSUM_LASTCHUNK(0xa0)
	CSUM_LASTCHUNK(0x90)
	CSUM_LASTCHUNK(0x80)
	CSUM_LASTCHUNK(0x70)
	CSUM_LASTCHUNK(0x60)
	CSUM_LASTCHUNK(0x50)
	CSUM_LASTCHUNK(0x40)
	CSUM_LASTCHUNK(0x30)
	CSUM_LASTCHUNK(0x20)
	CSUM_LASTCHUNK(0x10)
	CSUM_LASTCHUNK(0x00)
23:	brnz,pn		%o3, 26f		/*  CTI+IEU1	Group		*/
24:	 sllx		%o2, 32, %g1		/*  IEU0			*/
25:	addcc		%o2, %g1, %o0		/*  IEU1	Group		*/
	srlx		%o0, 32, %o0		/*  IEU0	Group (regdep)	*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o0, 1, %o0		/*  IEU1			*/
1:	retl					/*  CTI		Group brk forced*/
	 srl		%o0, 0, %o0		/*  IEU0			*/
26:	andcc		%o1, 8, %g0		/*  IEU1	Group		*/
	be,pn		%icc, 1f		/*  CTI				*/
	 ldx		[%o0], %g3		/*  Load			*/
	add		%o0, 8, %o0		/*  IEU0	Group		*/
	addcc		%g3, %o2, %o2		/*  IEU1	Group		*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	andcc		%o1, 4, %g0		/*  IEU1	Group		*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%g2			/*  IEU0			*/
	ld		[%o0], %g2		/*  Load			*/
	add		%o0, 4, %o0		/*  IEU0	Group		*/
	sllx		%g2, 32, %g2		/*  IEU0	Group		*/
1:	andcc		%o1, 2, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%o4			/*  IEU0	Group		*/
	lduh		[%o0], %o4		/*  Load			*/
	add		%o0, 2, %o0		/*  IEU1			*/
	sll		%o4, 16, %o4		/*  IEU0	Group		*/
1:	andcc		%o1, 1, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%o5			/*  IEU0	Group		*/
	ldub		[%o0], %o5		/*  Load			*/
	sll		%o5, 8, %o5		/*  IEU0	Group		*/
1:	or		%g2, %o4, %o4		/*  IEU1			*/
	or		%o5, %o4, %o4		/*  IEU0	Group (regdep)	*/
	addcc		%o4, %o2, %o2		/*  IEU1	Group (regdep)	*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	ba,pt		%xcc, 25b		/*  CTI		Group		*/
	 sllx		%o2, 32, %g1		/*  IEU0			*/
21:	srl		%o2, 0, %o2		/*  IEU0	Group		*/
	cmp		%o1, 0			/*  IEU1			*/
	be,pn		%icc, 24b		/*  CTI				*/
	 andcc		%o1, 4, %g0		/*  IEU1	Group		*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%g2			/*  IEU0			*/
	lduh		[%o0], %g3		/*  Load			*/
	lduh		[%o0+2], %g2		/*  Load	Group		*/
	add		%o0, 4, %o0		/*  IEU0	Group		*/
	sllx		%g3, 48, %g3		/*  IEU0	Group		*/
	sllx		%g2, 32, %g2		/*  IEU0	Group		*/
	or		%g3, %g2, %g2		/*  IEU0	Group		*/
1:	andcc		%o1, 2, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%o4			/*  IEU0	Group		*/
	lduh		[%o0], %o4		/*  Load			*/
	add		%o0, 2, %o0		/*  IEU1			*/
	sll		%o4, 16, %o4		/*  IEU0	Group		*/
1:	andcc		%o1, 1, %g0		/*  IEU1			*/
	be,a,pn		%icc, 1f		/*  CTI				*/
	 clr		%o5			/*  IEU0	Group		*/
	ldub		[%o0], %o5		/*  Load			*/
	sll		%o5, 8, %o5		/*  IEU0	Group		*/
1:	or		%g2, %o4, %o4		/*  IEU1			*/
	or		%o5, %o4, %o4		/*  IEU0	Group (regdep)	*/
	addcc		%o4, %o2, %o2		/*  IEU1	Group (regdep)	*/
	bcs,a,pn	%xcc, 1f		/*  CTI				*/
	 add		%o2, 1, %o2		/*  IEU0			*/
1:	ba,pt		%xcc, 25b		/*  CTI		Group		*/
	 sllx		%o2, 32, %g1		/*  IEU0			*/

	/* When buff is byte aligned and len is large, we backoff to
	 * this really slow handling.  The issue is that we cannot do
	 * the VIS stuff when buff is byte aligned as unaligned.c will
	 * not fix it up.
	 */
csump_really_slow:
	mov	%o0, %o3
	mov	%o1, %o4
	cmp	%o1, 0
	ble,pn	%icc, 9f
	 mov	0, %o0
	andcc	%o3, 1, %o5
	be,pt	%icc, 1f
	 sra	%o4, 1, %g3
	add	%o1, -1, %o4
	ldub	[%o3], %o0
	add	%o3, 1, %o3
	sra	%o4, 1, %g3
1:
	cmp	%g3, 0
	be,pt	%icc, 3f
	 and	%o4, 1, %g2
	and	%o3, 2, %g2
	brz,a,pt %g2, 1f
	 sra	%g3, 1, %g3
	add	%g3, -1, %g3
	add	%o4, -2, %o4
	lduh	[%o3], %g2
	add	%o3, 2, %o3
	add	%o0, %g2, %o0
	sra	%g3, 1, %g3
1:
	cmp	%g3, 0
	be,pt	%icc, 2f
	 and	%o4, 2, %g2
1:
	ld	[%o3], %g2
	addcc	%o0, %g2, %o0
	addx	%o0, %g0, %o0
	addcc	%g3, -1, %g3
	bne,pt	%icc, 1b
	 add	%o3, 4, %o3
	srl	%o0, 16, %o1
	sethi	%hi(64512), %g2
	or	%g2, 1023, %g2
	and	%o0, %g2, %g3
	add	%g3, %o1, %g3
	srl	%g3, 16, %o0
	and	%g3, %g2, %g2
	add	%g2, %o0, %g3
	sll	%g3, 16, %g3
	srl	%g3, 16, %o0
	and	%o4, 2, %g2
2:
	cmp	%g2, 0
	be,pt	%icc, 3f
	 and	%o4, 1, %g2
	lduh	[%o3], %g2
	add	%o3, 2, %o3
	add	%o0, %g2, %o0
	and	%o4, 1, %g2
3:
	cmp	%g2, 0
	be,pt	%icc, 1f
	 srl	%o0, 16, %o1
	ldub	[%o3], %g2
	sll	%g2, 8, %g2
	add	%o0, %g2, %o0
	srl	%o0, 16, %o1
1:
	sethi	%hi(64512), %g2
	or	%g2, 1023, %g2
	cmp	%o5, 0
	and	%o0, %g2, %g3
	add	%g3, %o1, %g3
	srl	%g3, 16, %o0
	and	%g3, %g2, %g2
	add	%g2, %o0, %g3
	sll	%g3, 16, %g3
	srl	%g3, 16, %o0
	srl	%g3, 24, %g3
	and	%o0, 255, %g2
	sll	%g2, 8, %g2
	bne,pt	%icc, 1f
	 or	%g3, %g2, %g2
9:
	mov	%o0, %g2
1:
	addcc	%g2, %o2, %g2
	addx	%g2, %g0, %g2
	retl
	 srl	%g2, 0, %o0
