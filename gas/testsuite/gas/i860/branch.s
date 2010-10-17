# Branches and calls

	.text

	bla	%r0,%r1,.Lsome_label1
	nop
	bla	%r5,%r31,.Lsome_label2
	nop
	bla	%r23,%r16,.Lsome_label1
	nop
	bla	%r4,%r19,.Lsome_label2
	nop

	bri	%r0	
	nop
	bri	%r1	
	nop
	bri	%r31	
	nop
	bri	%r1	
	nop
	bri	%r12	
	nop
	bri	%r19	
	nop

	calli	%r0	
	nop
	calli	%r1	
	nop
	calli	%r31	
	nop
	calli	%r5	
	nop
	calli	%r22	
	nop
	calli	%r9	
	nop

	br	.Lsome_label1
	nop
	br	.Lsome_label2
	nop
	br	some_fake_extern
	nop

	call	.Lcall_me_now
	nop
	call	.Lcall_me_anytime
	nop
	call	some_fake_extern
	nop

	bc	.+12
	bc	.Lsome_label1
	bc	some_fake_extern

	bc.t	.+0
	nop
	bc.t	.Lsome_label1
	nop
	bc.t	some_fake_extern
	nop

	bnc	.+12
	bnc	.Lsome_label1
	bnc	some_fake_extern

	bnc.t	.+0
	nop
	bnc.t	.Lsome_label1
	nop
	bnc.t	some_fake_extern
	nop


.Lsome_label1:
.Lcall_me_now:
	nop
	nop
.Lsome_label2:
.Lcall_me_anytime:
	nop
	nop

