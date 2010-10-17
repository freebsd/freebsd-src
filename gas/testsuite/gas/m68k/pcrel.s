	nop
lbl_b:	nop
	moveml	lbl_b,%a0-%a1
	moveml	%pc@(lbl_b),%a0-%a1
	moveml	%pc@(lbl_b,%d0),%a0-%a1
	lea	lbl_b,%a0
	lea	%pc@(lbl_b),%a0
	lea	%pc@(lbl_b-128),%a0
	lea	%pc@(lbl_b,%d0),%a0
	lea	%pc@(lbl_b:b,%d0),%a0
	lea	%pc@(lbl_b-.-2:b,%d0),%a0
	lea	%pc@(lbl_b:w,%d0),%a0
	lea	%pc@(lbl_b-.-2:w,%d0),%a0
	lea	%pc@(lbl_b:l,%d0),%a0
	lea	%pc@(lbl_b-.-2:l,%d0),%a0
	nop
	bsrl	lbl_a
	bsr	lbl_a
	bsrs	lbl_a
	jbsr	lbl_a
	nop
	lea	lbl_a,%a0
	lea	%pc@(lbl_a),%a0
	lea	%pc@(lbl_a+128),%a0
	lea	%pc@(lbl_a,%d0),%a0
	lea	%pc@(lbl_a:b,%d0),%a0
	lea	%pc@(lbl_a-.-2:b,%d0),%a0
	lea	%pc@(lbl_a:w,%d0),%a0
	lea	%pc@(lbl_a-.-2:w,%d0),%a0
	lea	%pc@(lbl_a:l,%d0),%a0
	lea	%pc@(lbl_a-.-2:l,%d0),%a0
	lea	%pc@(18:l,%d0),%a0
	lea	%pc@(10:w,%d0),%a0
	lea	%pc@(4:b,%d0),%a0
	nop
lbl_a:	nop
	nop
	lea	%pc@(.-126,%d0),%a0
	lea	%pc@(.-127,%d0),%a0
	lea	%pc@(.-32766,%d0),%a0
	lea	%pc@(.-32767,%d0),%a0
	nop
	lea	%pc@(.+129,%d0),%a0
	lea	%pc@(.+130,%d0),%a0
	lea	%pc@(.+32769,%d0),%a0
	lea	%pc@(.+32770,%d0),%a0
	nop
	lea	%pc@(.-32766),%a0
	lea	%pc@(.-32767),%a0
	nop
	lea	%pc@(.+32769),%a0
	lea	%pc@(.+32770),%a0
	nop
	lea	%pc@(undef),%a0
	lea	%pc@(undef,%d0),%a0
	nop
	lea	undef,%a0
	nop
	.p2align 3
