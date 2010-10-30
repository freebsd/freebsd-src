.text
.align 0

label:
	cps #15
	cpsid if
	cpsie if
	ldrex r2, [r4]
	ldrexne r4, [r8]
	mcrr2 p0, 12, r7, r5, c3
	mrrc2 p0, 12, r7, r5, c3
	pkhbt	r2, r5, r8
	pkhbt	r2, r5, r8, LSL #3
	pkhbtal	r2, r5, r8, LSL #3
	pkhbteq	r2, r5, r8, LSL #3	
	pkhtb	r2, r5, r8	@ Equivalent to pkhbt r2, r8, r5.
	pkhtb	r2, r5, r8, ASR #3
	pkhtbal	r2, r5, r8, ASR #3
	pkhtbeq	r2, r5, r8, ASR #3
	qadd16	r2, r4, r7
	qadd16ne r2, r4, r7	
	qadd8	r2, r4, r7
	qadd8ne r2, r4, r7
	qaddsubx	r2, r4, r7
	qaddsubxne	r2, r4, r7
	qsub16	 r2, r4, r7
	qsub16ne r2, r4, r7
	qsub8	 r2, r4, r7
	qsub8ne r2, r4, r7
	qsubaddx	 r2, r4, r7
	qsubaddx r2, r4, r7
	rev	r2, r4
	rev16	r2, r4
	rev16ne   r3, r5
	revne   r3, r5
	revsh	r2, r4
	revshne   r3, r5
	rfeda	r2
	rfedb	r2!
	rfeea	r2
	rfeed	r2!
	rfefa	r2!
	rfefd	r2
	rfeia	r2
	rfeib	r2!
	sadd16	 r2, r4, r7
	sadd16ne r2, r4, r7
	sxtah r2, r4, r5
	sxtah r2, r4, r5, ROR #8
	sxtahne r2, r4, r5
	sxtahne r2, r4, r5, ROR #8
	sadd8	 r2, r4, r7
	sadd8ne r2, r4, r7
	sxtab16 r2, r4, r5
	sxtab16 r2, r4, r5, ROR #8
	sxtab16ne r2, r4, r5
	sxtab16ne r2, r4, r5, ROR #8
	sxtab r2, r4, r5
	sxtab r2, r4, r5, ROR #8
	sxtabne r2, r4, r5
	sxtabne r2, r4, r5, ROR #8
	saddsubx	 r2, r4, r7
	saddsubxne r2, r4, r7
	sel r1, r2, r3
	selne r1, r2, r3
	setend be
	setend le
	shadd16	 r2, r4, r7
	shadd16ne r2, r4, r7
	shadd8	 r2, r4, r7
	shadd8ne r2, r4, r7
	shaddsubx	 r2, r4, r7
	shaddsubxne r2, r4, r7
	shsub16	 r2, r4, r7
	shsub16ne r2, r4, r7
	shsub8	 r2, r4, r7
	shsub8ne r2, r4, r7
	shsubaddx	 r2, r4, r7
	shsubaddxne r2, r4, r7
	smlad r1,r2,r3,r4
	smladle r1,r2,r3,r4
	smladx r1,r2,r3,r4
	smladxle r1,r2,r3,r4
	smlald r1,r2,r3,r4
	smlaldle r1,r2,r3,r4
	smlaldx r1,r2,r3,r4
	smlaldxle r1,r2,r3,r4
	smlsd r1,r2,r3,r4
	smlsdle r1,r2,r3,r4
	smlsdx r1,r2,r3,r4
	smlsdxle r1,r2,r3,r4	
	smlsld r1,r2,r3,r4	
	smlsldle r1,r2,r3,r4
	smlsldx r1,r2,r3,r4
	smlsldxle r1,r2,r3,r4	
	smmla r1,r2,r3,r4	
	smmlale r1,r2,r3,r4
	smmlar r1,r2,r3,r4
	smmlarle r1,r2,r3,r4	
	smmls r1,r2,r3,r4	
	smmlsle r1,r2,r3,r4
	smmlsr r1,r2,r3,r4
	smmlsrle r1,r2,r3,r4	
	smmul r1,r2,r3
	smmulle r1,r2,r3
	smmulr r1,r2,r3
	smmulrle r1,r2,r3
	smuad r1,r2,r3
	smuadle r1,r2,r3
	smuadx r1,r2,r3
	smuadxle r1,r2,r3
	smusd r1,r2,r3
	smusdle r1,r2,r3
	smusdx r1,r2,r3
	smusdxle r1,r2,r3
	srsia #16
	srsib #16!
	ssat r1, #1, r2
	ssat r1, #1, r2, ASR #2
	ssat r1, #1, r2, LSL #2
	ssat16 r1, #1, r1
	ssat16le r1, #1, r1
	ssub16	 r2, r4, r7
	ssub16ne r2, r4, r7
	ssub8	 r2, r4, r7
	ssub8ne r2, r4, r7
	ssubaddx	 r2, r4, r7
	ssubaddxne r2, r4, r7
	strex r1, r2, [r3]
	strexne r1, r2, [r3]
	sxth r2, r5
	sxth r2, r5, ROR #8
	sxthne r2, r5
	sxthne r2, r5, ROR #8
	sxtb16 r2, r5
	sxtb16 r2, r5, ROR #8
	sxtb16ne r2, r5
	sxtb16ne r2, r5, ROR #8
	sxtb r2, r5
	sxtb r2, r5, ROR #8
	sxtbne r2, r5
	sxtbne r2, r5, ROR #8
	uadd16	 r2, r4, r7
	uadd16ne r2, r4, r7
	uxtah r2, r3, r5
	uxtah r2, r3, r5, ROR #8
	uxtahne r2, r3, r5
	uxtahne r2, r3, r5, ROR #8
	uadd8	 r2, r4, r7
	uadd8ne r2, r4, r7
	uxtab16 r2, r3, r5
	uxtab16 r2, r3, r5, ROR #8
	uxtab16ne r2, r3, r5
	uxtab16ne r2, r3, r5, ROR #8
	uxtab r2, r3, r5
	uxtab r2, r3, r5, ROR #8
	uxtabne r2, r3, r5
	uxtabne r2, r3, r5, ROR #8
	uaddsubx	 r2, r4, r7
	uaddsubxne r2, r4, r7
	uhadd16	 r2, r4, r7
	uhadd16ne r2, r4, r7
	uhadd8	 r2, r4, r7
	uhadd8ne r2, r4, r7
	uhaddsubx	 r2, r4, r7
	uhaddsubxne r2, r4, r7
	uhsub16	 r2, r4, r7
	uhsub16ne r2, r4, r7
	uhsub8	 r2, r4, r7
	uhsub8ne r2, r4, r7
	uhsubaddx	 r2, r4, r7
	uhsubaddxne r2, r4, r7
	umaal	r1, r2, r3, r4
	umaalle	r1, r2, r3, r4	
	uqadd16	 r2, r4, r7
	uqadd16ne r2, r4, r7
	uqadd8	 r2, r4, r7
	uqadd8ne r2, r4, r7
	uqaddsubx	 r2, r4, r7
	uqaddsubxne r2, r4, r7
	uqsub16	 r2, r4, r7
	uqsub16ne r2, r4, r7
	uqsub8	 r2, r4, r7
	uqsub8ne r2, r4, r7
	uqsubaddx	 r2, r4, r7
	uqsubaddxne r2, r4, r7
	usad8 r1, r2, r3
	usad8ne r1, r2, r3
	usada8 r1, r2, r3, r4
	usada8ne r1, r2, r3, r4
	usat r1, #15, r2
	usat r1, #15, r2, ASR #4
	usat r1, #15, r2, LSL #4
	usat16 r1, #15, r2
	usat16le r1, #15, r2
	usatle r1, #15, r2
	usatle r1, #15, r2, ASR #4
	usatle r1, #15, r2, LSL #4
	usub16	 r2, r4, r7
	usub16ne r2, r4, r7
	usub8	 r2, r4, r7
	usub8ne r2, r4, r7
	usubaddx	 r2, r4, r7
	usubaddxne r2, r4, r7
	uxth r2, r5
	uxth r2, r5, ROR #8
	uxthne r2, r5
	uxthne r2, r5, ROR #8
	uxtb16 r2, r5
	uxtb16 r2, r5, ROR #8
	uxtb16ne r2, r5
	uxtb16ne r2, r5, ROR #8
	uxtb r2, r5
	uxtb r2, r5, ROR #8
	uxtbne r2, r5
	uxtbne r2, r5, ROR #8
	cpsie if, #10
	cpsie if, #21
	srsia sp, #16
	srsib sp!, #16
