/*
 * include/asm-sparc64/xor.h
 *
 * High speed xor_block operation for RAID4/5 utilizing the
 * UltraSparc Visual Instruction Set.
 *
 * Copyright (C) 1997, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *	Requirements:
 *	!(((long)dest | (long)sourceN) & (64 - 1)) &&
 *	!(len & 127) && len >= 256
 *
 * It is done in pure assembly, as otherwise gcc makes it a non-leaf
 * function, which is not what we want.
 */

#include <asm/pstate.h>
#include <asm/asi.h>

extern void xor_vis_2(unsigned long, unsigned long *, unsigned long *);
extern void xor_vis_3(unsigned long, unsigned long *, unsigned long *,
		      unsigned long *);
extern void xor_vis_4(unsigned long, unsigned long *, unsigned long *,
		      unsigned long *, unsigned long *);
extern void xor_vis_5(unsigned long, unsigned long *, unsigned long *,
		      unsigned long *, unsigned long *, unsigned long *);

#define _S(x) __S(x)
#define __S(x) #x
#define DEF(x) __asm__(#x " = " _S(x))

DEF(FPRS_FEF);
DEF(FPRS_DU);
DEF(ASI_BLK_P);

/* ??? We set and use %asi instead of using ASI_BLK_P directly because gas
   currently does not accept symbolic constants for the ASI specifier.  */

__asm__ ("\n\
	.text\n\
	.globl xor_vis_2\n\
	.type xor_vis_2,@function\n\
xor_vis_2:\n\
	rd	%fprs, %o5\n\
	andcc	%o5, FPRS_FEF|FPRS_DU, %g0\n\
	be,pt	%icc, 0f\n\
	 sethi	%hi(VISenter), %g1\n\
	jmpl	%g1 + %lo(VISenter), %g7\n\
	 add	%g7, 8, %g7\n\
0:	wr	%g0, FPRS_FEF, %fprs\n\
	rd	%asi, %g1\n\
	wr	%g0, ASI_BLK_P, %asi\n\
	membar	#LoadStore|#StoreLoad|#StoreStore\n\
	sub	%o0, 128, %o0\n\
	ldda	[%o1] %asi, %f0\n\
	ldda	[%o2] %asi, %f16\n\
\n\
2:	ldda	[%o1 + 64] %asi, %f32\n\
	fxor	%f0, %f16, %f16\n\
	fxor	%f2, %f18, %f18\n\
	fxor	%f4, %f20, %f20\n\
	fxor	%f6, %f22, %f22\n\
	fxor	%f8, %f24, %f24\n\
	fxor	%f10, %f26, %f26\n\
	fxor	%f12, %f28, %f28\n\
	fxor	%f14, %f30, %f30\n\
	stda	%f16, [%o1] %asi\n\
	ldda	[%o2 + 64] %asi, %f48\n\
	ldda	[%o1 + 128] %asi, %f0\n\
	fxor	%f32, %f48, %f48\n\
	fxor	%f34, %f50, %f50\n\
	add	%o1, 128, %o1\n\
	fxor	%f36, %f52, %f52\n\
	add	%o2, 128, %o2\n\
	fxor	%f38, %f54, %f54\n\
	subcc	%o0, 128, %o0\n\
	fxor	%f40, %f56, %f56\n\
	fxor	%f42, %f58, %f58\n\
	fxor	%f44, %f60, %f60\n\
	fxor	%f46, %f62, %f62\n\
	stda	%f48, [%o1 - 64] %asi\n\
	bne,pt	%xcc, 2b\n\
	 ldda	[%o2] %asi, %f16\n\
\n\
	ldda	[%o1 + 64] %asi, %f32\n\
	fxor	%f0, %f16, %f16\n\
	fxor	%f2, %f18, %f18\n\
	fxor	%f4, %f20, %f20\n\
	fxor	%f6, %f22, %f22\n\
	fxor	%f8, %f24, %f24\n\
	fxor	%f10, %f26, %f26\n\
	fxor	%f12, %f28, %f28\n\
	fxor	%f14, %f30, %f30\n\
	stda	%f16, [%o1] %asi\n\
	ldda	[%o2 + 64] %asi, %f48\n\
	membar	#Sync\n\
	fxor	%f32, %f48, %f48\n\
	fxor	%f34, %f50, %f50\n\
	fxor	%f36, %f52, %f52\n\
	fxor	%f38, %f54, %f54\n\
	fxor	%f40, %f56, %f56\n\
	fxor	%f42, %f58, %f58\n\
	fxor	%f44, %f60, %f60\n\
	fxor	%f46, %f62, %f62\n\
	stda	%f48, [%o1 + 64] %asi\n\
	membar	#Sync|#StoreStore|#StoreLoad\n\
	wr	%g1, %g0, %asi\n\
	retl\n\
	  wr	%g0, 0, %fprs\n\
	.size xor_vis_2, .-xor_vis_2\n\
\n\
\n\
	.globl xor_vis_3\n\
	.type xor_vis_3,@function\n\
xor_vis_3:\n\
	rd	%fprs, %o5\n\
	andcc	%o5, FPRS_FEF|FPRS_DU, %g0\n\
	be,pt	%icc, 0f\n\
	 sethi	%hi(VISenter), %g1\n\
	jmpl	%g1 + %lo(VISenter), %g7\n\
	 add	%g7, 8, %g7\n\
0:	wr	%g0, FPRS_FEF, %fprs\n\
	rd	%asi, %g1\n\
	wr	%g0, ASI_BLK_P, %asi\n\
	membar	#LoadStore|#StoreLoad|#StoreStore\n\
	sub	%o0, 64, %o0\n\
	ldda	[%o1] %asi, %f0\n\
	ldda	[%o2] %asi, %f16\n\
\n\
3:	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f48\n\
	fxor	%f2, %f18, %f50\n\
	add	%o1, 64, %o1\n\
	fxor	%f4, %f20, %f52\n\
	fxor	%f6, %f22, %f54\n\
	add	%o2, 64, %o2\n\
	fxor	%f8, %f24, %f56\n\
	fxor	%f10, %f26, %f58\n\
	fxor	%f12, %f28, %f60\n\
	fxor	%f14, %f30, %f62\n\
	ldda	[%o1] %asi, %f0\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	add	%o3, 64, %o3\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	subcc	%o0, 64, %o0\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	stda	%f48, [%o1 - 64] %asi\n\
	bne,pt	%xcc, 3b\n\
	 ldda	[%o2] %asi, %f16\n\
\n\
	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f48\n\
	fxor	%f2, %f18, %f50\n\
	fxor	%f4, %f20, %f52\n\
	fxor	%f6, %f22, %f54\n\
	fxor	%f8, %f24, %f56\n\
	fxor	%f10, %f26, %f58\n\
	fxor	%f12, %f28, %f60\n\
	fxor	%f14, %f30, %f62\n\
	membar	#Sync\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	stda	%f48, [%o1] %asi\n\
	membar	#Sync|#StoreStore|#StoreLoad\n\
	wr	%g1, %g0, %asi\n\
	retl\n\
	 wr	%g0, 0, %fprs\n\
	.size xor_vis_3, .-xor_vis_3\n\
\n\
\n\
	.globl xor_vis_4\n\
	.type xor_vis_4,@function\n\
xor_vis_4:\n\
	rd	%fprs, %o5\n\
	andcc	%o5, FPRS_FEF|FPRS_DU, %g0\n\
	be,pt	%icc, 0f\n\
	 sethi	%hi(VISenter), %g1\n\
	jmpl	%g1 + %lo(VISenter), %g7\n\
	 add	%g7, 8, %g7\n\
0:	wr	%g0, FPRS_FEF, %fprs\n\
	rd	%asi, %g1\n\
	wr	%g0, ASI_BLK_P, %asi\n\
	membar	#LoadStore|#StoreLoad|#StoreStore\n\
	sub	%o0, 64, %o0\n\
	ldda	[%o1] %asi, %f0\n\
	ldda	[%o2] %asi, %f16\n\
\n\
4:	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f16\n\
	fxor	%f2, %f18, %f18\n\
	add	%o1, 64, %o1\n\
	fxor	%f4, %f20, %f20\n\
	fxor	%f6, %f22, %f22\n\
	add	%o2, 64, %o2\n\
	fxor	%f8, %f24, %f24\n\
	fxor	%f10, %f26, %f26\n\
	fxor	%f12, %f28, %f28\n\
	fxor	%f14, %f30, %f30\n\
	ldda	[%o4] %asi, %f48\n\
	fxor	%f16, %f32, %f32\n\
	fxor	%f18, %f34, %f34\n\
	fxor	%f20, %f36, %f36\n\
	fxor	%f22, %f38, %f38\n\
	add	%o3, 64, %o3\n\
	fxor	%f24, %f40, %f40\n\
	fxor	%f26, %f42, %f42\n\
	fxor	%f28, %f44, %f44\n\
	fxor	%f30, %f46, %f46\n\
	ldda	[%o1] %asi, %f0\n\
	fxor	%f32, %f48, %f48\n\
	fxor	%f34, %f50, %f50\n\
	fxor	%f36, %f52, %f52\n\
	add	%o4, 64, %o4\n\
	fxor	%f38, %f54, %f54\n\
	fxor	%f40, %f56, %f56\n\
	fxor	%f42, %f58, %f58\n\
	subcc	%o0, 64, %o0\n\
	fxor	%f44, %f60, %f60\n\
	fxor	%f46, %f62, %f62\n\
	stda	%f48, [%o1 - 64] %asi\n\
	bne,pt	%xcc, 4b\n\
	 ldda	[%o2] %asi, %f16\n\
\n\
	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f16\n\
	fxor	%f2, %f18, %f18\n\
	fxor	%f4, %f20, %f20\n\
	fxor	%f6, %f22, %f22\n\
	fxor	%f8, %f24, %f24\n\
	fxor	%f10, %f26, %f26\n\
	fxor	%f12, %f28, %f28\n\
	fxor	%f14, %f30, %f30\n\
	ldda	[%o4] %asi, %f48\n\
	fxor	%f16, %f32, %f32\n\
	fxor	%f18, %f34, %f34\n\
	fxor	%f20, %f36, %f36\n\
	fxor	%f22, %f38, %f38\n\
	fxor	%f24, %f40, %f40\n\
	fxor	%f26, %f42, %f42\n\
	fxor	%f28, %f44, %f44\n\
	fxor	%f30, %f46, %f46\n\
	membar	#Sync\n\
	fxor	%f32, %f48, %f48\n\
	fxor	%f34, %f50, %f50\n\
	fxor	%f36, %f52, %f52\n\
	fxor	%f38, %f54, %f54\n\
	fxor	%f40, %f56, %f56\n\
	fxor	%f42, %f58, %f58\n\
	fxor	%f44, %f60, %f60\n\
	fxor	%f46, %f62, %f62\n\
	stda	%f48, [%o1] %asi\n\
	membar	#Sync|#StoreStore|#StoreLoad\n\
	wr	%g1, %g0, %asi\n\
	retl\n\
	 wr	%g0, 0, %fprs\n\
	.size xor_vis_4, .-xor_vis_4\n\
\n\
\n\
	.globl xor_vis_5\n\
	.type xor_vis_5,@function\n\
xor_vis_5:\n\
	mov	%o5, %g5\n\
	rd	%fprs, %o5\n\
	andcc	%o5, FPRS_FEF|FPRS_DU, %g0\n\
	be,pt	%icc, 0f\n\
	 sethi	%hi(VISenter), %g1\n\
	jmpl	%g1 + %lo(VISenter), %g7\n\
	 add	%g7, 8, %g7\n\
0:	wr	%g0, FPRS_FEF, %fprs\n\
	mov	%g5, %o5\n\
	rd	%asi, %g1\n\
	wr	%g0, ASI_BLK_P, %asi\n\
	membar	#LoadStore|#StoreLoad|#StoreStore\n\
	sub	%o0, 64, %o0\n\
	ldda	[%o1] %asi, %f0\n\
	ldda	[%o2] %asi, %f16\n\
\n\
5:	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f48\n\
	fxor	%f2, %f18, %f50\n\
	add	%o1, 64, %o1\n\
	fxor	%f4, %f20, %f52\n\
	fxor	%f6, %f22, %f54\n\
	add	%o2, 64, %o2\n\
	fxor	%f8, %f24, %f56\n\
	fxor	%f10, %f26, %f58\n\
	fxor	%f12, %f28, %f60\n\
	fxor	%f14, %f30, %f62\n\
	ldda	[%o4] %asi, %f16\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	add	%o3, 64, %o3\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	ldda	[%o5] %asi, %f32\n\
	fxor	%f48, %f16, %f48\n\
	fxor	%f50, %f18, %f50\n\
	add	%o4, 64, %o4\n\
	fxor	%f52, %f20, %f52\n\
	fxor	%f54, %f22, %f54\n\
	add	%o5, 64, %o5\n\
	fxor	%f56, %f24, %f56\n\
	fxor	%f58, %f26, %f58\n\
	fxor	%f60, %f28, %f60\n\
	fxor	%f62, %f30, %f62\n\
	ldda	[%o1] %asi, %f0\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	subcc	%o0, 64, %o0\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	stda	%f48, [%o1 - 64] %asi\n\
	bne,pt	%xcc, 5b\n\
	 ldda	[%o2] %asi, %f16\n\
\n\
	ldda	[%o3] %asi, %f32\n\
	fxor	%f0, %f16, %f48\n\
	fxor	%f2, %f18, %f50\n\
	fxor	%f4, %f20, %f52\n\
	fxor	%f6, %f22, %f54\n\
	fxor	%f8, %f24, %f56\n\
	fxor	%f10, %f26, %f58\n\
	fxor	%f12, %f28, %f60\n\
	fxor	%f14, %f30, %f62\n\
	ldda	[%o4] %asi, %f16\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	ldda	[%o5] %asi, %f32\n\
	fxor	%f48, %f16, %f48\n\
	fxor	%f50, %f18, %f50\n\
	fxor	%f52, %f20, %f52\n\
	fxor	%f54, %f22, %f54\n\
	fxor	%f56, %f24, %f56\n\
	fxor	%f58, %f26, %f58\n\
	fxor	%f60, %f28, %f60\n\
	fxor	%f62, %f30, %f62\n\
	membar	#Sync\n\
	fxor	%f48, %f32, %f48\n\
	fxor	%f50, %f34, %f50\n\
	fxor	%f52, %f36, %f52\n\
	fxor	%f54, %f38, %f54\n\
	fxor	%f56, %f40, %f56\n\
	fxor	%f58, %f42, %f58\n\
	fxor	%f60, %f44, %f60\n\
	fxor	%f62, %f46, %f62\n\
	stda	%f48, [%o1] %asi\n\
	membar	#Sync|#StoreStore|#StoreLoad\n\
	wr	%g1, %g0, %asi\n\
	retl\n\
	 wr	%g0, 0, %fprs\n\
	.size xor_vis_5, .-xor_vis_5\n\
");

static struct xor_block_template xor_block_VIS = {
        name: "VIS",
        do_2: xor_vis_2,
        do_3: xor_vis_3,
        do_4: xor_vis_4,
        do_5: xor_vis_5,
};

#define XOR_TRY_TEMPLATES       xor_speed(&xor_block_VIS)
