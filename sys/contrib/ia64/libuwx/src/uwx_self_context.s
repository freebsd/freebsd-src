/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef _LP64
#define SWIZZLE add
#define STPTR st8
#else
#define SWIZZLE addp4
#define STPTR st4
#endif

rRP	= r14
rPFS	= r15
rUNAT	= r16
rRNAT	= r17
rENV0	= r18
rENV1	= r19
rENV2	= r20
rNSLOT	= r21
rBSP	= r22
rPBSP	= r23
rRSC	= r24
rNATP	= r25
rBIAS	= r26
rRSC0	= r27
rTMP1	= r28
rTMP2	= r29
rTMP3	= r30
rTMP4	= r31
rTMP5	= r8

	.text
	.proc	uwx_self_init_context
	.global uwx_self_init_context
uwx_self_init_context:
	.prologue
	alloc	rPFS = ar.pfs, 1, 0, 0, 0
	mov	rUNAT = ar.unat
	.body
	SWIZZLE	rENV0 = r0, r32		// rENV0 = &env
	;;
	flushrs
	extr.u	rNSLOT = rPFS, 7, 7 	// nslots = pfs.sol
	mov	rRP = b0
	;;
	mov	rRSC = ar.rsc
	add	rENV1 = 120, rENV0	// rENV1 = &env->context.gr[0]
	add	rENV2 = 128, rENV0	// rENV2 = &env->context.gr[1]
	;;
	and	rRSC0 = -4, rRSC	// clear ar.rsc.mode
	adds	rNATP = 0x1f8, r0
	mov	rTMP1 = b1
	;;
	st8.spill [rENV1] = r4, 16	// env+120: r4
	st8.spill [rENV2] = r5, 16	// env+128: r5
	mov	rTMP2 = b2
	;;
	st8.spill [rENV1] = r6, 16	// env+136: r6
	st8.spill [rENV2] = r7, 16	// env+144: r7
	mov	rTMP3 = b3
	;;
	st8	[rENV1] = rTMP1, 16	// env+152: b1
	st8	[rENV2] = rTMP2, 16	// env+160: b2
	mov	rTMP1 = b4
	;;
	st8	[rENV1] = rTMP3, 16	// env+168: b3
	st8	[rENV2] = rTMP1, 16	// env+176: b4
	mov	rTMP2 = b5
	;;
	st8	[rENV1] = rTMP2		// env+184: b5
	mov	ar.rsc = rRSC0		// enforced lazy mode
	add	rENV1 = 8, rENV0
	;;
	mov	rRNAT = ar.rnat		// get copy of ar.rnat
	movl	rTMP1 = 0x7fec8f00000000 // valid_regs: ip, sp, bsp, cfm,
					// preds, rnat, unat, lc, grs, brs
	;;
	mov	ar.rsc = rRSC		// restore ar.rsc
	mov	rBSP = ar.bsp
	add	rTMP3 = 120, rENV0	// spill_loc = &env->context.gr[0]
	;;
	mov	rTMP2 = ar.unat
	nop
	extr.u	rTMP3 = rTMP3, 3, 6	// bitpos = spill_loc{8:3}
	;;
	or	rNATP = rBSP, rNATP	// natp = bsp | 0x1f8
	sub	rTMP4 = 64, rTMP3	// (64 - bitpos)
	shr	rTMP5 = rTMP2, rTMP3	// (unat >> bitpos)
	;;
	sub	rBIAS = rNATP, rBSP	// bias = (natp - bsp) ...
	nop
	shl	rTMP2 = rTMP2, rTMP4	// (unat << (64 - bitpos))
	;;
	or	rTMP2 = rTMP2, rTMP5	// rotate_right(unat, bitpos)
	extr.u	rBIAS = rBIAS, 3, 6	// ... div 8
	mov	rTMP4 = pr
	;;
	st8	[rENV0] = rTMP1, 16	// env+0: valid_regs mask
	st8	[rENV1] = rRP, 24	// env+8: ip (my rp)
	add	rBIAS = rNSLOT, rBIAS	// bias += nslots
	;;
	cmp.lt	p6, p0 = 63, rBIAS	// if (63 < bias) ...
	cmp.lt	p7, p0 = 126, rBIAS	// if (126 < bias) ...
	nop
	;;
	st8	[rENV0] = r12, 48	// env+16: sp
	st8	[rENV1] = rPFS, 40	// env+32: cfm (my pfs)
(p6)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
	st8	[rENV0] = rTMP4, 24	// env+64: preds
	st8	[rENV1] = rTMP2, 24	// env+72: priunat
(p7)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
	st8	[rENV0] = rRNAT, -64	// env+88: rnat
	st8	[rENV1] = rUNAT, 8	// env+96: unat
	dep.z	rTMP3 = rNSLOT, 3, 7 	// (nslots << 3)
	;;
	sub	rPBSP = rBSP, rTMP3	// prev_bsp = bsp - (nslots << 3)
	mov	rTMP3 = ar.fpsr
	mov	rTMP1 = ar.lc
	;;
	st8	[rENV0] = rPBSP		// env+24: bsp (my prev bsp)
	st8	[rENV1] = rTMP3, 8	// env+104: fpsr
	add	rENV2 = 320, rENV2	// rENV2 = &env->context.rstate
	;;
	st8	[rENV1] = rTMP1		// env+112: lc
	STPTR	[rENV2] = r0		// env+512: env->rstate = 0
	nop
	;;
	mov	ar.unat = rUNAT
	mov	ret0 = r0		// return UWX_OK
	br.ret.sptk	b0
	.endp

