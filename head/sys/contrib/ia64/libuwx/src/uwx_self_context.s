// Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

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
rMYPFS	= r9
rPSP	= r10

VALID_IP      = 1
VALID_SP      = 1 << 1
VALID_BSP     = 1 << 2
VALID_CFM     = 1 << 3
VALID_PREDS   = 1 << 7
VALID_PRIUNAT = 1 << 8
VALID_RNAT    = 1 << 10
VALID_UNAT    = 1 << 11
VALID_FPSR    = 1 << 12
VALID_LC      = 1 << 13
VALID_GRS     = 0xf << 16
VALID_BRS     = 0x1f << 20
VALID_BASIC4  = VALID_IP | VALID_SP | VALID_BSP | VALID_CFM
VALID_SPEC    = VALID_PREDS | VALID_PRIUNAT | VALID_RNAT | VALID_UNAT | VALID_FPSR | VALID_LC
VALID_REGS    = VALID_BASIC4 | VALID_SPEC | VALID_GRS | VALID_BRS
VALID_FRS     = 0xfffff
// valid_regs and valid_frs are separate unsigned int fields.
// In order to store them with a single st8, we need to know
// the endianness.
#ifdef __LITTLE_ENDIAN__
VALID_BITS   = (VALID_FRS << 32) | VALID_REGS
#else
VALID_BITS   = (VALID_REGS << 32) | VALID_FRS
#endif

	.text

// int uwx_self_init_context(struct uwx_env *env);
//
// Stores a snapshot of the caller's context in the uwx_env structure.

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
	add	rENV1 = 136, rENV0	// rENV1 = &env->context.gr[0]
	add	rENV2 = 144, rENV0	// rENV2 = &env->context.gr[1]
	;;
	and	rRSC0 = -4, rRSC	// clear ar.rsc.mode
	adds	rNATP = 0x1f8, r0
	mov	rTMP1 = b1
	;;
	st8.spill [rENV1] = r4, 16	// env+136: r4
	st8.spill [rENV2] = r5, 16	// env+144: r5
	mov	rTMP2 = b2
	;;
	st8.spill [rENV1] = r6, 16	// env+152: r6
	st8.spill [rENV2] = r7, 16	// env+160: r7
	mov	rTMP3 = b3
	;;
	st8	[rENV1] = rTMP1, 16	// env+168: b1
	st8	[rENV2] = rTMP2, 16	// env+176: b2
	mov	rTMP1 = b4
	;;
	st8	[rENV1] = rTMP3, 16	// env+184: b3
	st8	[rENV2] = rTMP1, 16	// env+192: b4
	mov	rTMP2 = b5
	;;
	st8	[rENV1] = rTMP2		// env+200: b5
	mov	ar.rsc = rRSC0		// enforced lazy mode
	add	rENV1 = 8, rENV0
	;;
	mov	rRNAT = ar.rnat		// get copy of ar.rnat
	movl	rTMP1 = VALID_BITS	// valid_regs: ip, sp, bsp, cfm,
					// preds, priunat, rnat, unat, fpsr,
					// lc, grs, brs
					// = 0x1ff3d8f00000000 
	;;
	mov	ar.rsc = rRSC		// restore ar.rsc
	mov	rBSP = ar.bsp
	add	rTMP3 = 136, rENV0	// spill_loc = &env->context.gr[0]
	;;
	mov	rTMP2 = ar.unat
	nop
	extr.u	rTMP3 = rTMP3, 3, 6	// bitpos = spill_loc{8:3}
	;;
	and	rBIAS = rBSP, rNATP	// bias = (bsp & 0x1f8) ...
	sub	rTMP4 = 64, rTMP3	// (64 - bitpos)
	shr	rTMP5 = rTMP2, rTMP3	// (unat >> bitpos)
	;;
	nop
	extr.u	rBIAS = rBIAS, 3, 6	//   ... div 8
	shl	rTMP2 = rTMP2, rTMP4	// (unat << (64 - bitpos))
	;;
	or	rTMP2 = rTMP2, rTMP5	// rotate_right(unat, bitpos)
	nop
	mov	rTMP4 = pr
	;;
	st8	[rENV0] = rTMP1, 16	// env+0: valid_regs mask
	st8	[rENV1] = rRP, 24	// env+8: ip (my rp)
	sub	rBIAS = rNSLOT, rBIAS	// bias = nslots - bias
	;;
	cmp.lt	p6, p0 = 0, rBIAS	// if (0 < bias) ...
	cmp.lt	p7, p0 = 63, rBIAS	// if (63 < bias) ...
	;;
	st8	[rENV0] = r12, 48	// env+16: sp
	st8	[rENV1] = rPFS, 40	// env+32: cfm (my pfs)
(p6)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
	st8	[rENV0] = rTMP4, 24	// env+64: preds
	st8	[rENV1] = rTMP2, 24	// env+72: priunat
(p7)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
	st8	[rENV0] = rRNAT, -64	// env+88: ar.rnat
	st8	[rENV1] = rUNAT, 8	// env+96: ar.unat
	dep.z	rTMP3 = rNSLOT, 3, 7 	// (nslots << 3)
	;;
	sub	rPBSP = rBSP, rTMP3	// prev_bsp = bsp - (nslots << 3)
	mov	rTMP3 = ar.fpsr
	mov	rTMP1 = ar.lc
	;;
	st8	[rENV0] = rPBSP, 184	// env+24: bsp (my prev bsp)
	st8	[rENV1] = rTMP3, 8	// env+104: ar.fpsr
	add	rENV2 = 320, rENV2	// rENV2 = &env->context.rstate
	;;
	st8	[rENV1] = rTMP1, 112	// env+112: ar.lc
	STPTR	[rENV2] = r0		// env+528: env->rstate = 0
	nop
	;;
	// THIS CODE NEEDS TO BE SCHEDULED!!!
	stf.spill [rENV0] = f2, 32	// env+208: f2
	stf.spill [rENV1] = f3, 32	// env+224: f3
	;;
	stf.spill [rENV0] = f4, 32	// env+240: f4
	stf.spill [rENV1] = f5, 32	// env+256: f5
	;;
	stf.spill [rENV0] = f16, 32	// env+272: f16
	stf.spill [rENV1] = f17, 32	// env+288: f17
	;;
	stf.spill [rENV0] = f18, 32	// env+304: f16
	stf.spill [rENV1] = f19, 32	// env+320: f17
	;;
	stf.spill [rENV0] = f20, 32	// env+336: f16
	stf.spill [rENV1] = f21, 32	// env+352: f17
	;;
	stf.spill [rENV0] = f22, 32	// env+368: f16
	stf.spill [rENV1] = f23, 32	// env+384: f17
	;;
	stf.spill [rENV0] = f24, 32	// env+400: f16
	stf.spill [rENV1] = f25, 32	// env+416: f17
	;;
	stf.spill [rENV0] = f26, 32	// env+432: f16
	stf.spill [rENV1] = f27, 32	// env+448: f17
	;;
	stf.spill [rENV0] = f28, 32	// env+464: f16
	stf.spill [rENV1] = f29, 32	// env+480: f17
	;;
	stf.spill [rENV0] = f30, 32	// env+496: f16
	stf.spill [rENV1] = f31, 32	// env+512: f17
	;;
	mov	ar.unat = rUNAT
	mov	ret0 = r0		// return UWX_OK
	br.ret.sptk b0
	.endp

// uwx_self_install_context(
//		struct uwx_env *env,
//		uint64_t r15,
//		uint64_t r16,
//		uint64_t r17,
//		uint64_t r18,
//		uint64_t ret
//		);
//
// Installs the given context, and sets the landing pad binding
// registers r15-r18 to the values given.
// Returns the value "ret" to the new context (for testing --
// when transferring to a landing pad, the new context won't
// care about the return value).

	.proc	uwx_self_install_context
	.global uwx_self_install_context
uwx_self_install_context:
	.prologue
	alloc	rMYPFS = ar.pfs, 6, 0, 0, 0
	.body
	SWIZZLE	rENV0 = r0, r32		// rENV0 = &env
	;;

	// THIS CODE NEEDS TO BE SCHEDULED!!!

	// Restore GR 4-7 and ar.unat
	add	rENV1 = 136, rENV0	// &env->context.gr[0]
	add	rENV2 = 72, rENV0	// &env->context.priunat
	;;
	ld8	rTMP2 = [rENV2], 24	// env+72: priunat
	extr.u	rTMP3 = rENV1, 3, 6	// bitpos = spill_loc{8:3}
	;;
	ld8	rUNAT = [rENV2], 48	// env+96: ar.unat
	sub	rTMP4 = 64, rTMP3	// (64 - bitpos)
	shl	rTMP5 = rTMP2, rTMP3	// (unat << bitpos)
	;;
	shr	rTMP2 = rTMP2, rTMP4	// (unat >> (64 - bitpos))
	;;
	or	rTMP2 = rTMP2, rTMP5	// rotate_left(unat, bitpos)
	;;
	mov	ar.unat = rTMP2		// put priunat in place
	;;
	ld8.fill r4 = [rENV1], 16	// env+136: r4
	ld8.fill r5 = [rENV2], 16	// env+144: r5
	;;
	ld8.fill r6 = [rENV1], 16	// env+152: r6
	ld8.fill r7 = [rENV2], 16	// env+160: r7
	;;
	mov	ar.unat = rUNAT		// restore real ar.unat

	// Restore BR 1-5
	ld8	rTMP1 = [rENV1], 16	// env+168: b1
	ld8	rTMP2 = [rENV2], 16	// env+176: b2
	;;
	ld8	rTMP3 = [rENV1], 16	// env+184: b3
	ld8	rTMP4 = [rENV2], -168	// env+192: b4
	mov	b1 = rTMP1
	;;
	ld8	rTMP1 = [rENV1], -168	// env+200: b5
	mov	b2 = rTMP2
	mov	b3 = rTMP3
	mov	b4 = rTMP4
	;;
	mov	b5 = rTMP1

	// Restore ar.bsp, ar.pfs, and ar.rnat
	ld8	rPFS = [rENV1], 56	// env+32: cfm (+saved ar.ec)
	mov	rRSC = ar.rsc
	adds	rBIAS = 0x1f8, r0
	;;
	flushrs
	ld8	rRNAT = [rENV1], -24	// env+88: ar.rnat
	ld8	rPBSP = [rENV2], 88	// env+24: prev_bsp
	and	rRSC0 = -4, rRSC	// clear ar.rsc.mode
	;;
	mov	ar.rsc = rRSC0		// enforced lazy mode
	extr.u	rNSLOT = rPFS, 7, 7 	// nslots = pfs.sol
	;;
	invala
	and	rBIAS = rPBSP, rBIAS	// bias = prev_bsp & 0x1f8 ...
	;;
	extr.u	rBIAS = rBIAS, 3, 6	// ... div 8
	;;
	add	rBIAS = rNSLOT, rBIAS	// bias += nslots
	;;
	cmp.lt	p6, p0 = 63, rBIAS	// if (63 < bias) ...
	cmp.lt	p7, p0 = 126, rBIAS	// if (126 < bias) ...
	;;
(p6)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
(p7)	add	rNSLOT = 1, rNSLOT	//   ... nslots++
	;;
	dep.z	rTMP3 = rNSLOT, 3, 7 	// (nslots << 3)
	;;
	add	rBSP = rPBSP, rTMP3	// bsp = prev_bsp + (nslots << 3)
	;;
	mov	ar.bspstore = rBSP	// restore ar.bsp
	;;
	mov	ar.rnat = rRNAT		// restore ar.rnat
	mov	ar.pfs = rPFS		// restore ar.pfs
	;;
	mov	ar.rsc = rRSC		// restore ar.rsc

	// Restore preds and ar.lc
	ld8	rTMP1 = [rENV1], -56	// env+64: preds
	ld8	rTMP2 = [rENV2], -96	// env+112: ar.lc
	;;
	mov	pr = rTMP1
	mov	ar.lc = rTMP2

	// Get previous sp and ip
	ld8	rRP = [rENV1], 96	// env+8: ip (my rp)
	ld8	rPSP = [rENV2], 112	// env+16: sp
	;;

	// Restore ar.fpsr and gp
	ld8	rTMP1 = [rENV1], 104	// env+104: ar.fpsr
	ld8	r1 = [rENV2], 96	// env+128: gp
	;;
	mov	ar.fpsr = rTMP1		// restore ar.fpsr

	// Restore FR 2-5 and 16-31
	ldf.fill f2 = [rENV1], 32	// env+208: f2
	ldf.fill f3 = [rENV2], 32	// env+224: f3
	;;
	ldf.fill f4 = [rENV1], 32	// env+240: f4
	ldf.fill f5 = [rENV2], 32	// env+256: f5
	;;
	ldf.fill f16 = [rENV1], 32	// env+272: f16
	ldf.fill f17 = [rENV2], 32	// env+288: f17
	;;
	ldf.fill f18 = [rENV1], 32	// env+304: f16
	ldf.fill f19 = [rENV2], 32	// env+320: f17
	;;
	ldf.fill f20 = [rENV1], 32	// env+336: f16
	ldf.fill f21 = [rENV2], 32	// env+352: f17
	;;
	ldf.fill f22 = [rENV1], 32	// env+368: f16
	ldf.fill f23 = [rENV2], 32	// env+384: f17
	;;
	ldf.fill f24 = [rENV1], 32	// env+400: f16
	ldf.fill f25 = [rENV2], 32	// env+416: f17
	;;
	ldf.fill f26 = [rENV1], 32	// env+432: f16
	ldf.fill f27 = [rENV2], 32	// env+448: f17
	;;
	ldf.fill f28 = [rENV1], 32	// env+464: f16
	ldf.fill f29 = [rENV2], 32	// env+480: f17
	;;
	ldf.fill f30 = [rENV1], 32	// env+496: f16
	ldf.fill f31 = [rENV2], 32	// env+512: f17

	// Set landing pad parameter registers
	mov	r15 = r33
	mov	r16 = r34
	mov	r17 = r35
	mov	r18 = r36

	// Restore previous sp and Return
	mov	ret0 = r37
	mov	sp = rPSP
	mov	b0 = rRP
	br.ret.sptk b0

	.endp
