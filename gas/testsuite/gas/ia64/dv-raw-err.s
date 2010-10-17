//	
// Detect RAW violations.  Cases taken from DV tables.
// This test is by no means complete but tries to hit the things that are 
// likely to be missed.
//	
.text
	.explicit
// AR[BSP]
	mov	ar.bspstore = r1
	mov	r0 = ar.bsp
	;;

// AR[BSPSTORE]	
	mov	ar.bspstore = r2
	mov	r3 = ar.bspstore
	;;
	
// AR[CCV]
	mov	ar.ccv = r4
	cmpxchg8.acq r5 = [r6],r7,ar.ccv
	;;
	
// AR[EC]	
	br.wtop.sptk	L
	mov	r8 = ar.ec
	;;

// AR[FPSR].sf0.controls 
	fsetc.s0	0x7f, 0x0f
	fpcmp.eq.s0	f2 = f3, f4
	;;

// AR[FPSR].sf1.controls
	fsetc.s1	0x7f, 0x0f
	fpcmp.eq.s1	f2 = f3, f4
	;;

// AR[FPSR].sf2.controls
	fsetc.s2	0x7f, 0x0f
	fpcmp.eq.s2	f2 = f3, f4
	;;

// AR[FPSR].sf3.controls
	fsetc.s3	0x7f, 0x0f
	fpcmp.eq.s3	f2 = f3, f4
	;;

// AR[FPSR].sf0.flags
	fpcmp.eq.s0	f2 = f3, f4
	fchkf.s0	L
	;;

// AR[FPSR].sf1.flags
	fpcmp.eq.s1	f2 = f3, f4
	fchkf.s1	L
	;;

// AR[FPSR].sf2.flags
	fpcmp.eq.s2	f2 = f3, f4
	fchkf.s2	L
	;;

// AR[FPSR].sf3.flags
	fpcmp.eq.s3	f2 = f3, f4
	fchkf.s3	L
	;;

// AR[FPSR].traps/rv
	mov	ar.fpsr = r0
	fcmp.eq.s3	p1, p2 = f5, f6
	;;

// AR[ITC]
	mov	ar.itc = r1
	mov	r2 = ar.itc
	;;

// AR[K]
	mov	ar.k1 = r3
	br.ia.sptk	b0
	;;
	
// AR[LC]
	br.cloop.sptk	L
	mov	r4 = ar.lc
	;;
	
// AR[PFS]
	mov	ar.pfs = r5
	epc

// AR[RNAT]	
	mov	ar.bspstore = r8
	mov	r9 = ar.rnat	
	;;
	
// AR[RSC]
	mov	ar.rsc = r10
	mov	r11 = ar.rnat
	;;	
	
// AR[UNAT]	
	mov	ar.unat = r12
	ld8.fill r13 = [r14]
	;;
	
// AR%

// BR%
	mov	b0 = r0
	mov	r0 = b0
	;;
	
// CFM	
	br.wtop.sptk	L
	fadd	f0 = f1, f32	// read from rotating register region
	;;
	
// CR[CMCV]
	mov	cr.cmcv = r1
	mov	r2 = cr.cmcv	
	;;

// CR[DCR]
	mov	cr.dcr = r3
	ld8.s	r4 = [r5]
	;;

// CR[EOI]
	
// CR[GPTA]
	mov	cr.gpta = r6
	thash	r7 = r8
	;;
	srlz.d

// CR[IFA]
	mov	cr.ifa = r9
	itc.i	r10
	;;

// CR[IFS]
	mov	cr.ifs = r11
	mov	r12 = cr.ifs
	;;

// CR[IHA]
	mov	cr.iha = r13
	mov	r14 = cr.iha
	;;

// CR[IIM]
	mov	cr.iim = r15
	mov	r16 = cr.iim
	;;

// CR[IIP] 
	mov	cr.iip = r17
	rfi
	;;

// CR[IIPA]
	mov	cr.iipa = r19
	mov	r20 = cr.iipa
	;;

// CR[IPSR]
	mov	cr.ipsr = r21
	rfi
	;;

// CR[IRR%]
	mov	r22 = cr.ivr
	mov	r23 = cr.irr0
	;;
	
// CR[ISR]
	mov	cr.isr = r24
	mov	r25 = cr.isr
	;;	
	
// CR[ITIR]
	mov	cr.itir = r26
	itc.d	r27
	;;	
	
// CR[ITM]
	mov	cr.itm = r28
	mov	r29 = cr.itm
	;;	
	
// CR[ITV]
	mov	cr.itv = r0
	mov	r1 = cr.itv
	;;	
	
// CR[IVR] (all writes are implicit in other resource usage)
	
// CR[IVA]
	mov	cr.iva = r0
	mov	r1 = cr.iva
	;;	
	
// CR[LID]
	mov	cr.lid = r0
	mov	r1 = cr.lid
	;;	
	srlz.d
	
// CR[LRR%]
	mov	cr.lrr0 = r0
	mov	r1 = cr.lrr0
	;;
	
// CR[PMV]
	mov	cr.pmv = r0
	mov	r1 = cr.pmv
	;;
	
// CR[PTA]
	mov	cr.pta = r0
	thash	r1 = r2
	;;
	
// CR[TPR]
	mov	cr.tpr = r0
	mov	r1 = cr.ivr	// data
	;;
	srlz.d
	mov	cr.tpr = r2
	mov	psr.l = r3	// other
	;;
	srlz.d
	
// DBR# 
	mov	dbr[r0] = r1
	mov	r2 = dbr[r3]
	;;	
	srlz.d
	mov	dbr[r4] = r5
	probe.r	r6 = r7, r8
	;;
	srlz.d
	
// DTC
	ptc.e	r0
	fc	r1
	;;
	srlz.d
	itr.i	itr[r2] = r3
	ptc.e	r4
	;;
	
// DTC_LIMIT/ITC_LIMIT 
	ptc.g	r0, r1		// NOTE: GAS automatically emits stops after 
	ptc.ga	r2, r3		//  ptc.g/ptc.ga, so this conflict is no     
	;;			//  longer possible in GAS-generated assembly
	srlz.d

// DTR
	itr.d	dtr[r0] = r1
	tak	r2 = r3
	;;
	srlz.d
	ptr.d	r4, r5
	tpa	r6 = r7
	;;
	srlz.d
	
// FR%
	ldfs.c.clr	f2 = [r1]
	mov		f3 = f2		// no DV here
	;;
	mov		f4 = f5
	mov		f6 = f4
	;;

// GR%
	ld8.c.clr	r0 = [r1]	// no DV here
	mov		r2 = r0		
	;;
	mov		r3 = r4
	mov		r5 = r3
	;;

// IBR#
	mov	ibr[r0] = r1
	mov	r2 = ibr[r3]
	;;

// InService		
	mov	cr.eoi = r0
	mov	r1 = cr.ivr
	;;
	srlz.d
	mov	r2 = cr.ivr
	mov	r3 = cr.ivr	// several DVs
	;;
	mov	cr.eoi = r4
	mov	cr.eoi = r5
	;;
	
// ITC		
	ptc.e	r0
	epc
	;;
	srlz.i
	;;
	
// ITC_LIMIT (see DTC_LIMIT)
	
// ITR	
	itr.i	itr[r0] = r1
	epc
	;;
	srlz.i
	;;
	
// PKR#
	mov	pkr[r0] = r1
	probe.r	r2 = r3, r4
	;;
	srlz.d
	mov	pkr[r5] = r6
	mov	r7 = pkr[r8]
	;;
	srlz.d
	
// PMC#
	mov	pmc[r0] = r1
	mov	r2 = pmc[r3]
	;;
	srlz.d
	mov	pmc[r4] = r5
	mov	r6 = pmd[r7]
	;;
	srlz.d
	
// PMD#
	mov	pmd[r0] = r1
	mov	r2 = pmd[r3]
	;;
	
// PR%, 1 - 15
	cmp.eq	p1, p2 = r0, r1	// pr-writer/pr-reader-nobr-nomovpr
(p1)	add	r2 = r3, r4	
	;;
	mov	pr = r5, 0xffff // mov-to-pr-allreg/pr-reader-nobr-nomovpr
(p2)	add	r6 = r7, r8	
	;;
	fcmp.eq p5, p6 = f2, f3 // pr-writer-fp/pr-reader-br
(p5)	br.cond.sptk	b0
	;;
	cmp.eq	p7, p8 = r11, r12
(p7)	br.cond.sptk	b1	// no DV here
	;;
	
// PR63
	br.wtop.sptk	L
(p63)	add	r0 = r1, r2
	;;
	fcmp.eq p62, p63 = f2, f3
(p63)	add	r3 = r4, r5	
	;;
	cmp.eq p62, p63 = r6, r7 // no DV here
(p63)	br.cond.sptk	b0
	;;	

// PSR.ac
	rum	(1<<3)
	ld8	r0 = [r1]
	;;

// PSR.be
	rum	(1<<1)
	ld8	r0 = [r1]
	;;
	
// PSR.bn
	bsw.0
	mov	r0 = r15	// no DV here, since gr < 16
	;;
	bsw.1			// GAS automatically emits a stop after bsw.n
	mov	r1 = r16	// so this conflict is avoided               
	;;
	
// PSR.cpl
	epc
	st8	[r0] = r1
	;;
	epc
	mov	r2 = ar.itc
	;;
	epc
	mov	ar.itc = r3
	;;
	epc
	mov	ar.rsc = r4
	;;
	epc
	mov	ar.k0 = r5
	;;
	epc
	mov	r6 = pmd[r7]
	;;
	epc
	mov	ar.bsp = r8	// no DV here
	;;
	epc
	mov	r9 = ar.bsp	// no DV here
	;;
	epc
	mov	cr.ifa = r10	// any mov-to/from-cr is a DV
	;;
	epc
	mov	r11 = cr.eoi	// any mov-to/from-cr is a DV
	;;

// PSR.da (rfi is the only writer)
// PSR.db (also ac,be,dt,pk)
	mov	psr.l = r0
	ld8	r1 = [r2]
	;;
	srlz.d

// PSR.dd (rfi is the only writer)
	
// PSR.dfh
	mov	psr.l = r0
	mov	f64 = f65
	;;
	srlz.d

// PSR.dfl
	mov	psr.l = r0
	mov	f3 = f4	
	;;
	srlz.d
	
// PSR.di
	rsm	(1<<22)
	mov	r0 = psr
	;;

// PSR.dt
	rsm	(1<<17)
	ld8	r0 = [r1]
	;;
	
// PSR.ed (rfi is the only writer)
// PSR.i
	ssm	(1<<14)
	mov	r0 = psr
	;;
	
// PSR.ia (no DV semantics)
// PSR.ic
	ssm	(1<<13)
	mov	r0 = psr
	;;
	srlz.d
	rsm	(1<<13)
	mov	r1 = cr.itir
	;;
	srlz.d
	rsm	(1<<13)
	mov	r1 = cr.irr0	// no DV here
	;;
	srlz.d

// PSR.id (rfi is the only writer)
// PSR.is (br.ia and rfi are the only writers)
// PSR.it (rfi is the only writer)
// PSR.lp
	mov	psr.l = r0
	br.ret.sptk	b0
	;;

// PSR.mc (rfi is the only writer)
// PSR.mfh
	mov	f32 = f33
	mov	r0 = psr
	;;

// PSR.mfl
	mov	f2 = f3
	mov	r0 = psr
	;;

// PSR.pk
	rsm	(1<<15)
	ld8	r0 = [r1]
	;;
	rsm	(1<<15)
	mov	r2 = psr
	;;

// PSR.pp
	rsm	(1<<21)
	mov	r0 = psr
	;;

// PSR.ri (no DV semantics)
// PSR.rt
	mov	psr.l = r0
	flushrs
	;;
	srlz.d

// PSR.si
	rsm	(1<<23)
	mov	r0 = ar.itc
	;;
	ssm	(1<<23)
	mov	r1 = ar.ec	// no DV here
	;;

// PSR.sp
	ssm	(1<<20)
	mov	r0 = pmd[r1]
	;;
	ssm	(1<<20)
	rum	0xff
	;;
	ssm	(1<<20)
	mov	r0 = rr[r1]
	;;

// PSR.ss (rfi is the only writer)
// PSR.tb
	mov	psr.l = r0
	chk.s	r0, L
	;;

// PSR.up
	rsm	(1<<2)
	mov	r0 = psr.um
	;;
	srlz.d

// RR#
	mov	rr[r0] = r1
	ld8	r2 = [r0]	// data
	;;
	mov	rr[r4] = r5
	mov	r6 = rr[r7]	// impliedf
	;;
	srlz.d
	;;
// RSE
	
// GR%, additional cases
// addl
	mov	r2 = r32
	addl	r3 = 12345, r2	// impliedf, IA64_OPND_R3_2
	;;
// postinc
	ld8	r2 = [r32], 8
	mov	r8 = r32	// impliedf
	;;

// PR%, 16 - 62
	cmp.eq	p21, p22 = r0, r1 // pr-writer/pr-reader-nobr-nomovpr
(p21)	add	r2 = r3, r4	
	;;
	mov	pr = r5, 0x1ffff // mov-to-pr-allreg/pr-reader-nobr-nomovpr
(p22)	add	r6 = r7, r8	
	;;
	mov	pr.rot = 0xffff0000 // mov-to-pr-rotreg/pr-reader-nobr-nomovpr
(p23)	add	r9 = r10, r11
	;;
	fcmp.eq p25, p26 = f2, f3 // pr-writer-fp/pr-reader-br
(p25)	br.cond.sptk	b0
	;;
	cmp.eq	p27, p28 = r11, r12
(p27)	br.cond.sptk	b1	// no DV here
	;;
	
L:	
