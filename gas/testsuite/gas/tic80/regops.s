;; Simple register forms
;; Those instructions which also use an immediate just use a constant.

	.text

	add		r3,r4,r5
	addu		r3,r4,r5
	and		r5,r4,r2
	and.tt		r5,r4,r2
	and.ff		r10,r12,r14
	and.ft		r10,r12,r14
	and.tf		r10,r12,r14
	bbo		r10,r8,lo.w
	bbo.a		r10,r8,eq.b
	bbz		r10,r8,ls.w
	bbz.a		r10,r8,hi.w
	bcnd		r4,r6,lt0.b
	bcnd.a		r4,r6,lt0.b
	br		r6
	br.a		r6
	brcr		10
	bsr		r6,r31
	bsr.a		r6,r31
	cmnd		r7
	cmp		r3,r4,r5
	dcachec		r8(r10)
	dcachef		r8(r10)
	dld.b		r4(r6),r8
	dld.h		r4(r6),r8
	dld		r4(r6),r8
	dld.d		r4(r6),r8
	dld.ub		r4(r6),r8
	dld.uh		r4(r6),r8
	dst.b		r4(r6),r8
	dst.h		r4(r6),r8
	dst		r4(r6),r8
	dst.d		r4(r6),r8
	etrap		r5
	exts		r3,31,r5,r6
	extu		r2,30,r5,r9
	fadd.sss	r2,r4,r6
	fadd.ssd	r2,r4,r6
	fadd.sdd	r2,r4,r6
	fadd.dsd	r2,r4,r6
	fadd.ddd	r2,r4,r6
	fcmp.ss		r4,r6,r8
	fcmp.sd		r4,r6,r8
	fcmp.ds		r4,r6,r8
	fcmp.dd		r4,r6,r8
	fdiv.sss	r2,r4,r6
	fdiv.ssd	r2,r4,r6
	fdiv.sdd	r2,r4,r6
	fdiv.dsd	r2,r4,r6
	fdiv.ddd	r2,r4,r6
	fmpy.sss	r2,r4,r6
	fmpy.ssd	r2,r4,r6
	fmpy.sdd	r2,r4,r6
	fmpy.dsd	r2,r4,r6
	fmpy.ddd	r2,r4,r6
	fmpy.iii	r2,r4,r6
	fmpy.uuu	r2,r4,r6
	frndm.ss	r4,r6
	frndm.sd	r4,r6
	frndm.si	r4,r6
	frndm.su	r4,r6
	frndm.ds	r2,r8
	frndm.dd	r2,r8
	frndm.di	r2,r8
	frndm.du	r2,r8
	frndm.is	r4,r6
	frndm.id	r4,r6
	frndm.us	r2,r8
	frndm.ud	r2,r8
	frndn.ss	r4,r6
	frndn.sd	r4,r6
	frndn.si	r4,r6
	frndn.su	r4,r6
	frndn.ds	r2,r8
	frndn.dd	r2,r8
	frndn.di	r2,r8
	frndn.du	r2,r8
	frndn.is	r4,r6
	frndn.id	r4,r6
	frndn.us	r2,r8
	frndn.ud	r2,r8
	frndp.ss	r4,r6
	frndp.sd	r4,r6
	frndp.si	r4,r6
	frndp.su	r4,r6
	frndp.ds	r2,r8
	frndp.dd	r2,r8
	frndp.di	r2,r8
	frndp.du	r2,r8
	frndp.is	r4,r6
	frndp.id	r4,r6
	frndp.us	r2,r8
	frndp.ud	r2,r8
	frndz.ss	r4,r6
	frndz.sd	r4,r6
	frndz.si	r4,r6
	frndz.su	r4,r6
	frndz.ds	r2,r8
	frndz.dd	r2,r8
	frndz.di	r2,r8
	frndz.du	r2,r8
	frndz.is	r4,r6
	frndz.id	r4,r6
	frndz.us	r2,r8
	frndz.ud	r2,r8
	fsqrt.ss	r6,r8
	fsqrt.sd	r6,r8
	fsqrt.dd	r6,r8
	fsub.sss	r2,r4,r6
	fsub.ssd	r2,r4,r6
	fsub.sdd	r2,r4,r6
	fsub.dsd	r2,r4,r6
	fsub.ddd	r2,r4,r6
	ins		r4,31,r8,r10
	jsr		r4(r6),r8
	jsr.a		r4(r6),r8
	ld.b		r4(r6),r8
	ld.h		r4(r6),r8
	ld		r4(r6),r8
	ld.d		r4(r6),r8
	ld.ub		r4(r6),r8
	ld.uh		r4(r6),r8
	lmo		r7,r8
	or		r1,r2,r3
	or.tt		r1,r2,r3
	or.ff		r1,r2,r3
	or.ft		r1,r2,r3
	or.tf		r1,r2,r3
	rdcr		r6,r4
	rmo		r4,r5
	rotl		r2,31,r8,r10
	rotr		r8,31,r2,r6
	shl		r4,31,r2,r6
	sl.dz		r4,12,r5,r6
	sl.dm		r4,12,r5,r6
	sl.ds		r4,12,r5,r6
	sl.ez		r4,12,r5,r6
	sl.em		r4,12,r5,r6
	sl.es		r4,12,r5,r6
	sl.iz		r4,12,r5,r6
	sl.im		r4,12,r5,r6
	sli.dz		r4,12,r5,r6
	sli.dm		r4,12,r5,r6
	sli.ds		r4,12,r5,r6
	sli.ez		r4,12,r5,r6
	sli.em		r4,12,r5,r6
	sli.es		r4,12,r5,r6
	sli.iz		r4,12,r5,r6
	sli.im		r4,12,r5,r6
	sr.dz		r4,12,r5,r6
	sr.dm		r4,12,r5,r6
	sr.ds		r4,12,r5,r6
	sr.ez		r4,12,r5,r6
	sr.em		r4,12,r5,r6
	sr.es		r4,12,r5,r6
	sr.iz		r4,12,r5,r6
	sr.im		r4,12,r5,r6
	sra		r4,32,r6,r8
	sri.dz		r4,12,r5,r6
	sri.dm		r4,12,r5,r6
	sri.ds		r4,12,r5,r6
	sri.ez		r4,12,r5,r6
	sri.em		r4,12,r5,r6
	sri.es		r4,12,r5,r6
	sri.iz		r4,12,r5,r6
	sri.im		r4,12,r5,r6
	srl		r4,32,r6,r8
	st.b		r4(r6),r8
	st.h		r4(r6),r8
	st		r4(r6),r8
	st.d		r4(r6),r8
	sub		r7,r8,r9
	subu		r7,r8,r9
	swcr		r8,r6,r4
	trap		r10
	vadd.ss		r2,r4,r4
	vadd.sd		r2,r6,r6
	vadd.dd		r2,r10,r10
;	vld0.s		r6
;	vld1.s		r7
;	vld0.d		r6
;	vld1.d		r8
;	vmac.sss	r7,r9,0,a3
;	vmac.sss	r7,r9,0,r10
;	vmac.sss	r7,r9,a1,a3
;	vmac.sss	r7,r9,a3,r10
;	vmac.ssd	r7,r9,0,a0
;	vmac.ssd	r7,r9,0,r10
;	vmac.ssd	r7,r9,a1,a2
;	vmac.ssd	r7,r9,a3,r10
;	vmpy.ss		r1,r3,r3
;	vmpy.sd		r5,r6,r6
;	vmpy.dd		r2,r4,r4
;	vmsc.sss	r7,r9,0,a0
;	vmsc.sss	r7,r9,0,r10
;	vmsc.sss	r7,r9,a0,a1
;	vmsc.sss	r7,r9,a3,r10
;	vmsc.ssd	r7,r9,0,a0
;	vmsc.ssd	r7,r9,0,r10
;	vmsc.ssd	r7,r9,a0,a1
;	vmsc.ssd	r7,r9,a3,r10
;	vmsub.ss	r6,a2,a4
;	vmsub.sd	r6,a2,a4
;	vmsub.ss	r4,a4,r6
;	vmsub.sd	r4,a4,r6
;	vrnd.si		r4,r6
;	vrnd.si		r4,a0
;	vrnd.su		r4,r6
;	vrnd.su		r4,a0
;	vrnd.ss		r4,r6
;	vrnd.ss		r4,a0
;	vrnd.sd		r4,r6
;	vrnd.sd		r4,a0
;	vrnd.di		r4,r6
;	vrnd.di		r4,a0
;	vrnd.du		r4,r6
;	vrnd.du		r4,a0
;	vrnd.ds		r4,r6
;	vrnd.ds		r4,a0
;	vrnd.dd		r4,r6
;	vrnd.dd		r4,a0
;	vrnd.is		r4,r6
;	vrnd.id		r4,r6
;	vrnd.us		r4,r6
;	vrnd.ud		r4,r6
;	vst.s		r6
;	vst.d		r6
;	vsub.ss		r2,r4,r6
;	vsub.sd		r2,r4,r6
;	vsub.dd		r2,r4,r6
	wrcr		r6,r5
	xnor		r5,r6,r7
	xor		r7,r8,r9
