//	
// Detect WAW violations.  Cases taken from DV tables.
//	
.text
	.explicit
// AR[BSP]
	mov	ar.bsp = r0
	mov	ar.bsp = r1
	;;
// AR[BSPSTORE]	
	mov	ar.bspstore = r2
	mov	ar.bspstore = r3
	;;
	
// AR[CCV]
	mov	ar.ccv = r4
	mov	ar.ccv = r4
	;;
	
// AR[EC]	
	br.wtop.sptk	L
	mov	ar.ec = r0
	;;

// AR[FPSR].sf0.controls 
	mov		ar.fpsr = r0
	fsetc.s0	0x7f, 0x0f
	;;

// AR[FPSR].sf1.controls
	mov		ar.fpsr = r0
	fsetc.s1	0x7f, 0x0f
	;;

// AR[FPSR].sf2.controls
	mov		ar.fpsr = r0
	fsetc.s2	0x7f, 0x0f
	;;

// AR[FPSR].sf3.controls
	mov		ar.fpsr = r0
	fsetc.s3	0x7f, 0x0f
	;;

// AR[FPSR].sf0.flags
	fcmp.eq.s0	p1, p2 = f3, f4
	fcmp.eq.s0	p3, p4 = f3, f4	// no DV here
	;;
	fcmp.eq.s0	p1, p2 = f3, f4
	fclrf.s0
	;;

// AR[FPSR].sf1.flags
	fcmp.eq.s1	p1, p2 = f3, f4
	fcmp.eq.s1	p3, p4 = f3, f4	// no DV here
	;;
	fcmp.eq.s1	p1, p2 = f3, f4
	fclrf.s1
	;;

// AR[FPSR].sf2.flags
	fcmp.eq.s2	p1, p2 = f3, f4
	fcmp.eq.s2	p3, p4 = f3, f4	// no DV here
	;;
	fcmp.eq.s2	p1, p2 = f3, f4
	fclrf.s2
	;;

// AR[FPSR].sf3.flags
	fcmp.eq.s3	p1, p2 = f3, f4
	fcmp.eq.s3	p3, p4 = f3, f4	// no DV here
	;;
	fcmp.eq.s3	p1, p2 = f3, f4
	fclrf.s3
	;;

// AR[FPSR].traps/rv plus all controls/flags
	mov	ar.fpsr = r0
	mov	ar.fpsr = r0
	;;

// AR[ITC]
	mov	ar.itc = r1
	mov	ar.itc = r1
	;;

// AR[K]
	mov	ar.k2 = r3
	mov	ar.k2 = r3
	;;
	
// AR[LC]
	br.cloop.sptk	L
	mov	ar.lc = r0
	;;
	
// AR[PFS]
	mov	ar.pfs = r0
	br.call.sptk	b0 = L
	;;

// AR[RNAT] (see also AR[BSPSTORE])
	mov	ar.rnat = r8
	mov	ar.rnat = r8
	;;
	
// AR[RSC]
	mov	ar.rsc = r10
	mov	ar.rsc = r10
	;;	
	
// AR[UNAT]	
	mov	ar.unat = r12
	st8.spill	[r0] = r1
	;;
	
// AR%
	mov	ar48 = r0
	mov	ar48 = r0
	;;

// BR%
	mov	b1 = r0
	mov	b1 = r1
	;;
	
// CFM (and others)
	br.wtop.sptk	L
	br.wtop.sptk	L
	;;
	
// CR[CMCV]
	mov	cr.cmcv = r1
	mov	cr.cmcv = r2
	;;

// CR[DCR]
	mov	cr.dcr = r3
	mov	cr.dcr = r3
	;;

// CR[EOI] (and InService)
	mov	cr.eoi = r0
	mov	cr.eoi = r0
	;;
	srlz.d
	
// CR[GPTA]
	mov	cr.gpta = r6
	mov	cr.gpta = r7
	;;

// CR[IFA]
	mov	cr.ifa = r9
	mov	cr.ifa = r10
	;;

// CR[IFS]
	mov	cr.ifs = r11
	cover
	;;

// CR[IHA]
	mov	cr.iha = r13
	mov	cr.iha = r14
	;;

// CR[IIM]
	mov	cr.iim = r15
	mov	cr.iim = r16
	;;

// CR[IIP] 
	mov	cr.iip = r17
	mov	cr.iip = r17
	;;

// CR[IIPA]
	mov	cr.iipa = r19
	mov	cr.iipa = r20
	;;

// CR[IPSR]
	mov	cr.ipsr = r21
	mov	cr.ipsr = r22
	;;

// CR[IRR%] (and others)
	mov	r2 = cr.ivr
	mov	r3 = cr.ivr
	;;
	
// CR[ISR]
	mov	cr.isr = r24
	mov	cr.isr = r25
	;;	
	
// CR[ITIR]
	mov	cr.itir = r26
	mov	cr.itir = r27
	;;	
	
// CR[ITM]
	mov	cr.itm = r28
	mov	cr.itm = r29
	;;	
	
// CR[ITV]
	mov	cr.itv = r0
	mov	cr.itv = r1
	;;	
	
// CR[IVA]
	mov	cr.iva = r0
	mov	cr.iva = r1
	;;	
	
// CR[IVR] (no explicit writers)
	
// CR[LID]
	mov	cr.lid = r0
	mov	cr.lid = r1
	;;
	
// CR[LRR%]
	mov	cr.lrr0 = r0
	mov	cr.lrr1 = r0 // no DV here
	;;
	mov	cr.lrr0 = r0
	mov	cr.lrr0 = r0
	;;
	
// CR[PMV]
	mov	cr.pmv = r0
	mov	cr.pmv = r1
	;;
	
// CR[PTA]
	mov	cr.pta = r0
	mov	cr.pta = r1
	;;
	
// CR[TPR]
	mov	cr.tpr = r0
	mov	cr.tpr = r1
	;;
	
// DBR# 
	mov	dbr[r1] = r1
	mov	dbr[r1] = r2
	;;
	srlz.d
	
// DTC
	ptc.e	r0
	ptc.e	r1	// no DVs here
	;;
	ptc.e	r0	// (and others)
	itc.i	r0
	;;
	srlz.d
	
// DTC_LIMIT
	ptc.g	r0, r1		// NOTE: GAS automatically emits stops after 
	ptc.ga	r2, r3		//  ptc.g/ptc.ga, so this conflict is no     
	;;			//  longer possible in GAS-generated assembly
	srlz.d
	
// DTR 
	itr.d	dtr[r0] = r1	// (and others)
	ptr.d	r2, r3
	;;
	srlz.d
	
// FR%
	mov		f3 = f2
	ldfs.c.clr	f3 = [r1]
	;;

// GR%
	mov		r2 = r0		
	ld8.c.clr	r2 = [r1]
	;;

// IBR#
	mov	ibr[r0] = r2
	mov	ibr[r1] = r2
	;;

// InService		
	mov	cr.eoi = r0
	mov	r1 = cr.ivr
	;;
	srlz.d
	
// ITC		
	ptc.e	r0
	itc.i	r1
	;;
	srlz.i
	;;
	
// ITR	
	itr.i	itr[r0] = r1
	ptr.i	r2, r3
	;;
	srlz.i
	;;
	
// PKR#
	.reg.val r1, 0x1
	.reg.val r2, ~0x1
	mov	pkr[r1] = r1
	mov	pkr[r2] = r1	// no DV here
	;;
	mov	pkr[r1] = r1
	mov	pkr[r1] = r1
	;;
	
// PMC#
	mov	pmc[r3] = r1
	mov	pmc[r4] = r1
	;;
	
// PMD#
	mov	pmd[r3] = r1
	mov	pmd[r4] = r1
	;;
	
// PR%, 1 - 15
	cmp.eq	p1, p0 = r0, r1
	cmp.eq	p1, p0 = r2, r3
	;;
	fcmp.eq p1, p2 = f2, f3
	fcmp.eq p1, p3 = f2, f3
	;;
	cmp.eq.and p1, p2 = r0, r1
	cmp.eq.or  p1, p3 = r2, r3
	;;
	cmp.eq.or  p1, p3 = r2, r3
	cmp.eq.and p1, p2 = r0, r1
	;;
	cmp.eq.and p1, p2 = r0, r1
	cmp.eq.and p1, p3 = r2, r3 // no DV here
	;;
	cmp.eq.or p1, p2 = r0, r1
	cmp.eq.or p1, p3 = r2, r3 // no DV here
	;;
	
// PR63
	br.wtop.sptk	L
	br.wtop.sptk	L
	;;
	cmp.eq	p63, p0 = r0, r1
	cmp.eq	p63, p0 = r2, r3
	;;
	fcmp.eq p63, p2 = f2, f3
	fcmp.eq p63, p3 = f2, f3
	;;
	cmp.eq.and p63, p2 = r0, r1
	cmp.eq.or  p63, p3 = r2, r3
	;;
	cmp.eq.or  p63, p3 = r2, r3
	cmp.eq.and p63, p2 = r0, r1
	;;
	cmp.eq.and p63, p2 = r0, r1
	cmp.eq.and p63, p3 = r2, r3 // no DV here
	;;
	cmp.eq.or p63, p2 = r0, r1
	cmp.eq.or p63, p3 = r2, r3 // no DV here
	;;

// PSR.ac
	rum	(1<<3)
	rum	(1<<3)
	;;

// PSR.be
	rum	(1<<1)
	rum	(1<<1)
	;;
	
// PSR.bn
	bsw.0			// GAS automatically emits a stop after bsw.n
	bsw.0			// so this conflict is avoided               
	;;

// PSR.cpl
	epc
	br.ret.sptk	b0
	;;

// PSR.da (rfi is the only writer)
// PSR.db (and others)
	mov	psr.l = r0
	mov	psr.l = r1
	;;
	srlz.d

// PSR.dd (rfi is the only writer)
	
// PSR.dfh
	ssm	(1<<19)
	ssm	(1<<19)
	;;
	srlz.d

// PSR.dfl
	ssm	(1<<18)
	ssm	(1<<18)
	;;
	srlz.d
	
// PSR.di
	rsm	(1<<22)
	rsm	(1<<22)
	;;

// PSR.dt
	rsm	(1<<17)
	rsm	(1<<17)
	;;
	
// PSR.ed (rfi is the only writer)
// PSR.i
	ssm	(1<<14)
	ssm	(1<<14)
	;;
	
// PSR.ia (no DV semantics)
// PSR.ic
	ssm	(1<<13)
	ssm	(1<<13)
	;;

// PSR.id (rfi is the only writer)
// PSR.is (br.ia and rfi are the only writers)
// PSR.it (rfi is the only writer)
// PSR.lp (see PSR.db)

// PSR.mc (rfi is the only writer)
// PSR.mfh
	mov	f32 = f33
	mov	r10 = psr
	;;
	ssm	(1<<5)
	ssm	(1<<5)
	;;
	ssm	(1<<5)
	mov	psr.um = r10
	;;
	rum	(1<<5)
	rum	(1<<5)
	;;
	mov	f32 = f33
	mov	f34 = f35	// no DV here
	;;

// PSR.mfl
	mov	f2 = f3
	mov	r10 = psr
	;;
	ssm	(1<<4)
	ssm	(1<<4)
	;;
	ssm	(1<<4)
	mov	psr.um = r10
	;;
	rum	(1<<4)
	rum	(1<<4)
	;;
	mov	f2 = f3
	mov	f4 = f5	// no DV here
	;;

// PSR.pk
	rsm	(1<<15)
	rsm	(1<<15)
	;;

// PSR.pp
	rsm	(1<<21)
	rsm	(1<<21)
	;;

// PSR.ri (no DV semantics)
// PSR.rt (see PSR.db)

// PSR.si
	rsm	(1<<23)
	ssm	(1<<23)
	;;

// PSR.sp
	ssm	(1<<20)
	rsm	(1<<20)
	;;
	srlz.d

// PSR.ss (rfi is the only writer)
// PSR.tb (see PSR.db)

// PSR.up
	rsm	(1<<2)
	rsm	(1<<2)
	;;
	rum	(1<<2)
	mov	psr.um = r0
	;;

// RR#
	mov	rr[r2] = r1
	mov	rr[r2] = r3
	;;

// PR, additional cases (or.andcm and and.orcm interaction)
	cmp.eq.or.andcm	p6, p7 = 1, r32
	cmp.eq.or.andcm p6, p7 = 5, r36	// no DV here
	;;
	cmp.eq.and.orcm	p6, p7 = 1, r32
	cmp.eq.and.orcm p6, p7 = 5, r36	// no DV here
	;;
	cmp.eq.or.andcm	p63, p7 = 1, r32
	cmp.eq.or.andcm p63, p7 = 5, r36 // no DV here
	;;
	cmp.eq.or.andcm	p6, p63 = 1, r32
	cmp.eq.or.andcm p6, p63 = 5, r36 // no DV here
	;;
	cmp.eq.and.orcm	p63, p7 = 1, r32
	cmp.eq.and.orcm p63, p7 = 5, r36 // no DV here
	;;
	cmp.eq.and.orcm	p6, p63 = 1, r32
	cmp.eq.and.orcm p6, p63 = 5, r36 // no DV here
	;;
	cmp.eq.or.andcm	p6, p7 = 1, r32
	cmp.eq.and.orcm p6, p7 = 5, r36	
	;;
	cmp.eq.or.andcm	p63, p7 = 1, r32
	cmp.eq.and.orcm p63, p7 = 5, r36	
	;;
	cmp.eq.or.andcm	p6, p63 = 1, r32
	cmp.eq.and.orcm p6, p63 = 5, r36	
	;;

// PR%, 16 - 62
	cmp.eq	p21, p0 = r0, r1
	cmp.eq	p21, p0 = r2, r3
	;;
	fcmp.eq p21, p22 = f2, f3
	fcmp.eq p21, p23 = f2, f3
	;;
	cmp.eq.and p21, p22 = r0, r1
	cmp.eq.or  p21, p23 = r2, r3
	;;
	cmp.eq.or  p21, p23 = r2, r3
	cmp.eq.and p21, p22 = r0, r1
	;;
	cmp.eq.and p21, p22 = r0, r1
	cmp.eq.and p21, p23 = r2, r3 // no DV here
	;;
	cmp.eq.or p21, p22 = r0, r1
	cmp.eq.or p21, p23 = r2, r3 // no DV here
	;;

// RSE

L:
