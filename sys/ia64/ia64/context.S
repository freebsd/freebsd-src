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
 *
 * $FreeBSD$
 */

#include <machine/asm.h>
#include <assym.s>

	.text

/*
 * void restorectx(struct pcb *)
 */
ENTRY(restorectx, 1)
{	.mmi
	invala
	mov		ar.rsc=0
	add		r31=8,r32
	;;
}
{	.mmi
	ld8		r12=[r32]		// sp
	ld8		r16=[r31],16		// unat (before)
	add		r30=16,r32
	;;
}
{	.mmi
	ld8		r17=[r30],16		// rp
	ld8		r18=[r31],16		// pr
	add		r14=SIZEOF_SPECIAL,r32
	;;
}
{	.mmi
	ld8		r19=[r30],16		// pfs
	ld8		r20=[r31],16		// bspstore
	mov		rp=r17
	;;
}
{	.mmi
	loadrs
	ld8		r21=[r30],16		// rnat
	mov		pr=r18,0x1fffe
	;;
}
{	.mmi
	ld8		r17=[r14],8		// unat (after)
	mov		ar.bspstore=r20
	mov		ar.pfs=r19
	;;
}
{	.mmi
	mov		ar.unat=r17
	mov		ar.rnat=r21
	add		r15=8,r14
	;;
}
{	.mmi
	ld8.fill	r4=[r14],16		// r4
	ld8.fill	r5=[r15],16		// r5
	nop		0
	;;
}
{	.mmi
	ld8.fill	r6=[r14],16		// r6
	ld8.fill	r7=[r15],16		// r7
	nop		1
	;;
}
{	.mmi
	mov		ar.unat=r16
	mov		ar.rsc=3
	nop		2
}
{	.mmi
	ld8		r17=[r14],16		// b1
	ld8		r18=[r15],16		// b2
	nop		3
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
{	.mmi
	ldf.fill	f28=[r14],32
	ldf.fill	f29=[r15],32
	add		r8=1,r0
	;;
}
{	.mmb
	ldf.fill	f30=[r14]
	ldf.fill	f31=[r15]
	br.ret.sptk	rp
	;;
}
END(restorectx)

/*
 * void swapctx(struct pcb *old, struct pcb *new)
 */

ENTRY(swapctx, 2)
{	.mmi
	mov		ar.rsc=0
	mov		r16=ar.unat
	add		r31=8,r32
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
	cmp.eq		p15,p0=0,r33
	;;
}
{	.mmi
	mov		r16=ar.rnat
(p15)	mov		ar.rsc=3
	add		r30=SIZEOF_SPECIAL-(6*8),r32
	;;
}
{	.mmi
	st8		[r32]=r16,SIZEOF_SPECIAL-(4*8)		// rnat
	st8		[r31]=r0,SIZEOF_SPECIAL-(6*8)		// __spare
	mov		r16=b1
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
	stf.spill	[r32]=f30
(p15)	add		r8=0,r0
	;;
}
{	.mmb
	stf.spill	[r31]=f31
	mf
(p15)	br.ret.sptk	rp
	;;
}
{	.mfb
	mov		r32=r33
	nop		0
	br.sptk		restorectx
	;;
}
END(swapctx)

/*
 * save_callee_saved(struct _callee_saved *)
 */
ENTRY(save_callee_saved, 1)
{	.mii
	nop		0
	add		r14=8,r32
	add		r15=16,r32
	;;
}
{	.mmi
	.mem.offset	8,0
	st8.spill	[r14]=r4,16		// r4
	.mem.offset	16,0
	st8.spill	[r15]=r5,16		// r5
	mov		r16=b1
	;;
}
{	.mmi
	.mem.offset	24,0
	st8.spill	[r14]=r6,16		// r6
	.mem.offset	32,0
	st8.spill	[r15]=r7,16		// r7
	mov		r17=b2
	;;
}
{	.mmi
	st8		[r14]=r16,16		// b1
	mov		r18=ar.unat
	mov		r19=b3
	;;
}
{	.mmi
	st8		[r32]=r18		// nat (after)
	st8		[r15]=r17,16		// b2
	mov		r16=b4
	;;
}
{	.mmi
	st8		[r14]=r19,16		// b3
	st8		[r15]=r16,16		// b4
	mov		r17=b5
	;;
}
{	.mfi
	st8		[r14]=r17,16		// b5
	nop		0
	mov		r16=ar.lc
	;;
}
{	.mmb
	st8		[r15]=r16		// ar.lc
	st8		[r14]=r0		// __spare
	br.ret.sptk	rp
	;;
}
END(save_callee_saved)

/*
 * restore_callee_saved(struct _callee_saved *)
 */
ENTRY(restore_callee_saved, 1)
{	.mmi
	ld8		r30=[r32],16		// nat (after)
	;;
	mov		ar.unat=r30
	add		r31=-8,r32
	;;
}
{	.mmb
	ld8.fill	r4=[r31],16		// r4
	ld8.fill	r5=[r32],16		// r5
	nop		0
	;;
}
{	.mmb
	ld8.fill	r6=[r31],16		// r6
	ld8.fill	r7=[r32],16		// r7
	nop		0
	;;
}
{	.mmi
	ld8		r30=[r31],16		// b1
	ld8		r29=[r32],16		// b2
	nop		0
	;;
}
{	.mmi
	ld8		r28=[r31],16		// b3
	ld8		r27=[r32],16		// b4
	mov		b1=r30
	;;
}
{	.mii
	ld8		r26=[r31]		// b5
	mov		b2=r29
	mov		b3=r28
	;;
}
{	.mii
	ld8		r25=[r32]		// lc
	mov		b4=r27
	mov		b5=r26
	;;
}
{	.mib
	nop		0
	mov		ar.lc=r25
	br.ret.sptk	rp
	;;
}
END(restore_callee_saved)

/*
 * save_callee_saved_fp(struct _callee_saved_fp *)
 */
ENTRY(save_callee_saved_fp, 1)
	add		r31=16,r32
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
	stf.spill	[r31]=f29,32
	stf.spill	[r32]=f30
	;;
	stf.spill	[r31]=f31
	br.ret.sptk	rp
	;;
END(save_callee_saved_fp)

/*
 * restore_callee_saved_fp(struct _callee_saved_fp *)
 */
ENTRY(restore_callee_saved_fp, 1)
	add		r31=16,r32
	ldf.fill	f2=[r32],32
	;;
	ldf.fill	f3=[r31],32
	ldf.fill	f4=[r32],32
	;;
	ldf.fill	f5=[r31],32
	ldf.fill	f16=[r32],32
	;;
	ldf.fill	f17=[r31],32
	ldf.fill	f18=[r32],32
	;;
	ldf.fill	f19=[r31],32
	ldf.fill	f20=[r32],32
	;;
	ldf.fill	f21=[r31],32
	ldf.fill	f22=[r32],32
	;;
	ldf.fill	f23=[r31],32
	ldf.fill	f24=[r32],32
	;;
	ldf.fill	f25=[r31],32
	ldf.fill	f26=[r32],32
	;;
	ldf.fill	f27=[r31],32
	ldf.fill	f28=[r32],32
	;;
	ldf.fill	f29=[r31],32
	ldf.fill	f30=[r32]
	;;
	ldf.fill	f31=[r31]
	br.ret.sptk	rp
	;;
END(restore_callee_saved_fp)

/*
 * save_high_fp(struct _high_fp *)
 */
ENTRY(save_high_fp, 1)
	rsm		psr.dfh
	;;
	srlz.d
	add		r31=16,r32
	stf.spill	[r32]=f32,32
	;;
	stf.spill	[r31]=f33,32
	stf.spill	[r32]=f34,32
	;;
	stf.spill	[r31]=f35,32
	stf.spill	[r32]=f36,32
	;;
	stf.spill	[r31]=f37,32
	stf.spill	[r32]=f38,32
	;;
	stf.spill	[r31]=f39,32
	stf.spill	[r32]=f40,32
	;;
	stf.spill	[r31]=f41,32
	stf.spill	[r32]=f42,32
	;;
	stf.spill	[r31]=f43,32
	stf.spill	[r32]=f44,32
	;;
	stf.spill	[r31]=f45,32
	stf.spill	[r32]=f46,32
	;;
	stf.spill	[r31]=f47,32
	stf.spill	[r32]=f48,32
	;;
	stf.spill	[r31]=f49,32
	stf.spill	[r32]=f50,32
	;;
	stf.spill	[r31]=f51,32
	stf.spill	[r32]=f52,32
	;;
	stf.spill	[r31]=f53,32
	stf.spill	[r32]=f54,32
	;;
	stf.spill	[r31]=f55,32
	stf.spill	[r32]=f56,32
	;;
	stf.spill	[r31]=f57,32
	stf.spill	[r32]=f58,32
	;;
	stf.spill	[r31]=f59,32
	stf.spill	[r32]=f60,32
	;;
	stf.spill	[r31]=f61,32
	stf.spill	[r32]=f62,32
	;;
	stf.spill	[r31]=f63,32
	stf.spill	[r32]=f64,32
	;;
	stf.spill	[r31]=f65,32
	stf.spill	[r32]=f66,32
	;;
	stf.spill	[r31]=f67,32
	stf.spill	[r32]=f68,32
	;;
	stf.spill	[r31]=f69,32
	stf.spill	[r32]=f70,32
	;;
	stf.spill	[r31]=f71,32
	stf.spill	[r32]=f72,32
	;;
	stf.spill	[r31]=f73,32
	stf.spill	[r32]=f74,32
	;;
	stf.spill	[r31]=f75,32
	stf.spill	[r32]=f76,32
	;;
	stf.spill	[r31]=f77,32
	stf.spill	[r32]=f78,32
	;;
	stf.spill	[r31]=f79,32
	stf.spill	[r32]=f80,32
	;;
	stf.spill	[r31]=f81,32
	stf.spill	[r32]=f82,32
	;;
	stf.spill	[r31]=f83,32
	stf.spill	[r32]=f84,32
	;;
	stf.spill	[r31]=f85,32
	stf.spill	[r32]=f86,32
	;;
	stf.spill	[r31]=f87,32
	stf.spill	[r32]=f88,32
	;;
	stf.spill	[r31]=f89,32
	stf.spill	[r32]=f90,32
	;;
	stf.spill	[r31]=f91,32
	stf.spill	[r32]=f92,32
	;;
	stf.spill	[r31]=f93,32
	stf.spill	[r32]=f94,32
	;;
	stf.spill	[r31]=f95,32
	stf.spill	[r32]=f96,32
	;;
	stf.spill	[r31]=f97,32
	stf.spill	[r32]=f98,32
	;;
	stf.spill	[r31]=f99,32
	stf.spill	[r32]=f100,32
	;;
	stf.spill	[r31]=f101,32
	stf.spill	[r32]=f102,32
	;;
	stf.spill	[r31]=f103,32
	stf.spill	[r32]=f104,32
	;;
	stf.spill	[r31]=f105,32
	stf.spill	[r32]=f106,32
	;;
	stf.spill	[r31]=f107,32
	stf.spill	[r32]=f108,32
	;;
	stf.spill	[r31]=f109,32
	stf.spill	[r32]=f110,32
	;;
	stf.spill	[r31]=f111,32
	stf.spill	[r32]=f112,32
	;;
	stf.spill	[r31]=f113,32
	stf.spill	[r32]=f114,32
	;;
	stf.spill	[r31]=f115,32
	stf.spill	[r32]=f116,32
	;;
	stf.spill	[r31]=f117,32
	stf.spill	[r32]=f118,32
	;;
	stf.spill	[r31]=f119,32
	stf.spill	[r32]=f120,32
	;;
	stf.spill	[r31]=f121,32
	stf.spill	[r32]=f122,32
	;;
	stf.spill	[r31]=f123,32
	stf.spill	[r32]=f124,32
	;;
	stf.spill	[r31]=f125,32
	stf.spill	[r32]=f126
	;;
	stf.spill	[r31]=f127
	ssm		psr.dfh
	;;
	srlz.d
	br.ret.sptk	rp
	;;
END(save_high_fp)

/*
 * restore_high_fp(struct _high_fp *)
 */
ENTRY(restore_high_fp, 1)
	rsm		psr.dfh
	;;
	srlz.d
	add		r31=16,r32
	ldf.fill	f32=[r32],32
	;;
	ldf.fill	f33=[r31],32
	ldf.fill	f34=[r32],32	
	;;
	ldf.fill	f35=[r31],32
	ldf.fill	f36=[r32],32
	;;
	ldf.fill	f37=[r31],32
	ldf.fill	f38=[r32],32
	;;
	ldf.fill	f39=[r31],32
	ldf.fill	f40=[r32],32
	;;
	ldf.fill	f41=[r31],32
	ldf.fill	f42=[r32],32
	;;
	ldf.fill	f43=[r31],32
	ldf.fill	f44=[r32],32
	;;
	ldf.fill	f45=[r31],32
	ldf.fill	f46=[r32],32
	;;
	ldf.fill	f47=[r31],32
	ldf.fill	f48=[r32],32
	;;
	ldf.fill	f49=[r31],32
	ldf.fill	f50=[r32],32
	;;
	ldf.fill	f51=[r31],32
	ldf.fill	f52=[r32],32
	;;
	ldf.fill	f53=[r31],32
	ldf.fill	f54=[r32],32
	;;
	ldf.fill	f55=[r31],32
	ldf.fill	f56=[r32],32
	;;
	ldf.fill	f57=[r31],32
	ldf.fill	f58=[r32],32
	;;
	ldf.fill	f59=[r31],32
	ldf.fill	f60=[r32],32
	;;
	ldf.fill	f61=[r31],32
	ldf.fill	f62=[r32],32
	;;
	ldf.fill	f63=[r31],32
	ldf.fill	f64=[r32],32
	;;
	ldf.fill	f65=[r31],32
	ldf.fill	f66=[r32],32
	;;
	ldf.fill	f67=[r31],32
	ldf.fill	f68=[r32],32
	;;
	ldf.fill	f69=[r31],32
	ldf.fill	f70=[r32],32
	;;
	ldf.fill	f71=[r31],32
	ldf.fill	f72=[r32],32
	;;
	ldf.fill	f73=[r31],32
	ldf.fill	f74=[r32],32
	;;
	ldf.fill	f75=[r31],32
	ldf.fill	f76=[r32],32
	;;
	ldf.fill	f77=[r31],32
	ldf.fill	f78=[r32],32
	;;
	ldf.fill	f79=[r31],32
	ldf.fill	f80=[r32],32
	;;
	ldf.fill	f81=[r31],32
	ldf.fill	f82=[r32],32
	;;
	ldf.fill	f83=[r31],32
	ldf.fill	f84=[r32],32
	;;
	ldf.fill	f85=[r31],32
	ldf.fill	f86=[r32],32
	;;
	ldf.fill	f87=[r31],32
	ldf.fill	f88=[r32],32
	;;
	ldf.fill	f89=[r31],32
	ldf.fill	f90=[r32],32
	;;
	ldf.fill	f91=[r31],32
	ldf.fill	f92=[r32],32
	;;
	ldf.fill	f93=[r31],32
	ldf.fill	f94=[r32],32
	;;
	ldf.fill	f95=[r31],32
	ldf.fill	f96=[r32],32
	;;
	ldf.fill	f97=[r31],32
	ldf.fill	f98=[r32],32
	;;
	ldf.fill	f99=[r31],32
	ldf.fill	f100=[r32],32
	;;
	ldf.fill	f101=[r31],32
	ldf.fill	f102=[r32],32
	;;
	ldf.fill	f103=[r31],32
	ldf.fill	f104=[r32],32
	;;
	ldf.fill	f105=[r31],32
	ldf.fill	f106=[r32],32
	;;
	ldf.fill	f107=[r31],32
	ldf.fill	f108=[r32],32
	;;
	ldf.fill	f109=[r31],32
	ldf.fill	f110=[r32],32
	;;
	ldf.fill	f111=[r31],32
	ldf.fill	f112=[r32],32
	;;
	ldf.fill	f113=[r31],32
	ldf.fill	f114=[r32],32
	;;
	ldf.fill	f115=[r31],32
	ldf.fill	f116=[r32],32
	;;
	ldf.fill	f117=[r31],32
	ldf.fill	f118=[r32],32
	;;
	ldf.fill	f119=[r31],32
	ldf.fill	f120=[r32],32
	;;
	ldf.fill	f121=[r31],32
	ldf.fill	f122=[r32],32
	;;
	ldf.fill	f123=[r31],32
	ldf.fill	f124=[r32],32
	;;
	ldf.fill	f125=[r31],32
	ldf.fill	f126=[r32]
	;;
	ldf.fill	f127=[r31]
	ssm		psr.dfh
	;;
	srlz.d
	br.ret.sptk	rp
	;;
END(restore_high_fp)
