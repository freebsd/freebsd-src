/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#define	SIZEOF_SPECIAL	(18*8)

/*
 * int _ia64_restore_context(mcontext_t *mc, intptr_t val, intptr_t *loc);
 */
ENTRY(_ia64_restore_context, 3)
{	.mmi
	invala
	mov		ar.rsc=0xc
	add		r32=16,r32
	;;
}
{	.mmi
	loadrs
	ld8		r12=[r32]		// sp
	add		r31=8,r32
	;;
}
{	.mii
	ld8		r16=[r31],16		// unat (before)
	add		r30=16,r32
	add		r14=SIZEOF_SPECIAL,r32
	;;
}
{	.mmi
	ld8		r17=[r30],16		// rp
	ld8		r18=[r31],16		// pr
	mov		r2=r33
	;;
}
{	.mmi
	ld8		r19=[r30],16		// pfs
	ld8		r20=[r31],32		// bspstore
	mov		rp=r17
	;;
}
{	.mmi
	ld8		r21=[r30],32		// rnat
	ld8		r22=[r31],16		// rsc
	mov		pr=r18,0x1fffe
	;;
}
{	.mmi
	ld8		r23=[r30]		// fpsr
	ld8		r24=[r31]		// psr		-- not used
	mov		r3=r34
	;;
}
{	.mmi
	ld8		r17=[r14],8		// unat (after)
	mov		ar.bspstore=r20
	cmp.ne		p15,p0=r0,r3
	;;
}
{	.mmi
	mov		ar.rnat=r21
	mov		ar.unat=r17
	add		r15=8,r14
	;;
}
{	.mmi
	ld8.fill	r4=[r14],16		// r4
	ld8.fill	r5=[r15],16		// r5
	mov		ar.pfs=r19
	;;
}
{	.mmi
	ld8.fill	r6=[r14],16		// r6
	ld8.fill	r7=[r15],16		// r7
	nop		0
	;;
}
{	.mmi
	mov		ar.unat=r16
	mov		ar.rsc=r22
	nop		0
}
{	.mmi
	ld8		r17=[r14],16		// b1
	ld8		r18=[r15],16		// b2
	nop		0
	;;
}
{	.mmi
	ld8		r19=[r14],16		// b3
	ld8		r20=[r15],16		// b4
	mov		b1=r17
	;;
}
{	.mmi
	ld8		r16=[r14],24		// b5
	ld8		r17=[r15],32		// lc
	mov		b2=r18
	;;
}
{	.mmi
	ldf.fill	f2=[r14],32
	ldf.fill	f3=[r15],32
	mov		b3=r19
	;;
}
{	.mmi
	ldf.fill	f4=[r14],32
	ldf.fill	f5=[r15],32
	mov		b4=r20
	;;
}
{	.mmi
	ldf.fill	f16=[r14],32
	ldf.fill	f17=[r15],32
	mov		b5=r16
	;;
}
{	.mmi
	ldf.fill	f18=[r14],32
	ldf.fill	f19=[r15],32
	mov		ar.lc=r17
	;;
}
	ldf.fill	f20=[r14],32
	ldf.fill	f21=[r15],32
	;;
	ldf.fill	f22=[r14],32
	ldf.fill	f23=[r15],32
	;;
	ldf.fill	f24=[r14],32
	ldf.fill	f25=[r15],32
	;;
	ldf.fill	f26=[r14],32
	ldf.fill	f27=[r15],32
	;;
	ldf.fill	f28=[r14],32
	ldf.fill	f29=[r15],32
	;;
	ldf.fill	f30=[r14],32+24
	ldf.fill	f31=[r15],24+24
	;;
	ld8		r8=[r14],16
	ld8		r9=[r15],16
	;;
	ld8		r10=[r14]
	ld8		r11=[r15]
	;;
{	.mmb
(p15)	st8		[r3]=r2
	mov		ar.fpsr=r23
	br.ret.sptk	rp
	;;
}
END(_ia64_restore_context)

/*
 * int _ia64_save_context(mcontext_t *mc);
 */
ENTRY(_ia64_save_context, 1)
{	.mmi
	mov		r14=ar.rsc
	mov		r15=ar.fpsr
	add		r31=8,r32
	;;
}
{	.mmi
	st8		[r32]=r0,16
	st8		[r31]=r0,16
	nop		0
	;;
}
{	.mmi
	mov		ar.rsc=0xc
	mov		r16=ar.unat
	nop		0
	;;
}
{	.mmi
	flushrs
	st8		[r32]=sp,16		// sp
	mov		r17=rp
	;;
}
{	.mmi
	st8		[r31]=r16,16		// unat (before)
	st8		[r32]=r17,16		// rp
	mov		r16=pr
	;;
}
{	.mmi
	st8		[r31]=r16,16		// pr
	mov		r17=ar.bsp
	mov		r16=ar.pfs
	;;
}
{	.mmi
	st8		[r32]=r16,16		// pfs
	st8		[r31]=r17,16		// bspstore
	nop		0
	;;
}
{	.mmi
	mov		r16=ar.rnat
	mov		ar.rsc=r14
	add		r30=SIZEOF_SPECIAL-(6*8),r32
	;;
}
{	.mmi
	st8		[r32]=r16,16		// rnat
	st8		[r31]=r0,16		// __spare
	nop		0
	;;
}
{	.mmi
	st8		[r32]=r13,16		// tp		-- not used
	st8		[r31]=r14,16		// rsc
	mov		r16=b1
	;;
}
{	.mmi
	st8		[r32]=r15,10*8		// fpr
	st8		[r31]=r0,8*8		// psr
	nop		0
	;;
}
	/* callee_saved */
{	.mmi
	.mem.offset	8,0
	st8.spill	[r31]=r4,16		// r4
	.mem.offset	16,0
	st8.spill	[r32]=r5,16		// r5
	mov		r17=b2
	;;
}
{	.mmi
	.mem.offset	24,0
	st8.spill	[r31]=r6,16		// r6
	.mem.offset	32,0
	st8.spill	[r32]=r7,16		// r7
	mov		r18=b3
	;;
}
{	.mmi
	st8		[r31]=r16,16		// b1
	mov		r16=ar.unat
	mov		r19=b4
	;;
}
{	.mmi
	st8		[r30]=r16		// unat (after)
	st8		[r32]=r17,16		// b2
	mov		r16=b5
	;;
}
{	.mmi
	st8		[r31]=r18,16		// b3
	st8		[r32]=r19,16		// b4
	mov		r17=ar.lc
	;;
}
	st8		[r31]=r16,16		// b5
	st8		[r32]=r17,16		// lc
	;;
	st8		[r31]=r0,24		// __spare
	stf.spill	[r32]=f2,32
	;;
	stf.spill	[r31]=f3,32
	stf.spill	[r32]=f4,32
	;;
	stf.spill	[r31]=f5,32
	stf.spill	[r32]=f16,32
	;;
	stf.spill	[r31]=f17,32
	stf.spill	[r32]=f18,32
	;;
	stf.spill	[r31]=f19,32
	stf.spill	[r32]=f20,32
	;;
	stf.spill	[r31]=f21,32
	stf.spill	[r32]=f22,32
	;;
	stf.spill	[r31]=f23,32
	stf.spill	[r32]=f24,32
	;;
	stf.spill	[r31]=f25,32
	stf.spill	[r32]=f26,32
	;;
	stf.spill	[r31]=f27,32
	stf.spill	[r32]=f28,32
	;;
{	.mmi
	stf.spill	[r31]=f29,32
	stf.spill	[r32]=f30,32+24
	add		r14=1,r0
	;;
}
{	.mmi
	stf.spill	[r31]=f31,24+24
	st8		[r32]=r14,16		// r8
	add		r8=0,r0
	;;
}
	st8		[r31]=r0,16		// r9
	st8		[r32]=r0		// r10
	;;
{	.mmb
	st8		[r31]=r0		// r11
	mf
	br.ret.sptk	rp
	;;
}
END(_ia64_save_context)
