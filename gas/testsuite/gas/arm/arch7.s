	# ARMV7 instructions
	.text
	.arch armv7r
label1:
	pli	[r6, r8]
	pli	[r9, r7]
	pli	[r0, r1, lsl #2]
	pli	[r5]
	pli	[r5, #4095]
	pli	[r5, #-4095]

	dbg	#0
	dbg	#15
	dmb
	dmb	sy
	dsb
	dsb	sy
	dsb	un
	dsb	st
	dsb	unst
	isb
	isb	sy
	.thumb
	.thumb_func
label2:
	pli	[r6, r8]
	pli	[r9, r7]
	pli	[r0, r1, lsl #2]
	pli	[r5]
	pli	[r5, #4095]
	pli	[r5, #-255]
	pli	[pc, #4095]
	pli	[pc, #-4095]

	dbg	#0
	dbg	#15
	dmb
	dmb	sy
	dsb
	dsb	sy
	dsb	un
	dsb	st
	dsb	unst
	isb
	isb	sy

	sdiv	r6, r9, r12
	sdiv	r9, r6, r3
	udiv	r9, r6, r3
	udiv	r6, r9, r12
	.arch armv7m
	mrs	r0, apsr
	mrs	r0, iapsr
	mrs	r0, eapsr
	mrs	r0, psr
	mrs	r0, ipsr
	mrs	r0, epsr
	mrs	r0, iepsr
	mrs	r0, msp
	mrs	r0, psp
	mrs	r0, primask
	mrs	r0, basepri
	mrs	r0, basepri_max
	mrs	r0, faultmask
	mrs	r0, control
	msr	apsr, r0
	msr	iapsr, r0
	msr	eapsr, r0
	msr	psr, r0
	msr	ipsr, r0
	msr	epsr, r0
	msr	iepsr, r0
	msr	msp, r0
	msr	psp, r0
	msr	primask, r0
	msr	basepri, r0
	msr	basepri_max, r0
	msr	faultmask, r0
	msr	control, r0
