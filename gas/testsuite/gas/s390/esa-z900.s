.text
foo:
	brcl	15,.
	jgo	.
	jgh	.
	jgp	.
	jgnle	.
	jgl	.
	jgm	.
	jgnhe	.
	jglh	.
	jgne	.
	jgnz	.
	jge	.
	jgz	.
	jgnlh	.
	jghe	.
	jgnl	.
	jgnm	.
	jgle	.
	jgnh	.
	jgnp	.
	jgno	.
	jg	.
	brasl	%r6,.
	tam
	sam24
	sam31
	stfl	4095(%r5)
	lrvr	%r6,%r9
	epsw	%r6,%r9
	mlr	%r6,%r9
	dlr	%r6,%r9
	alcr	%r6,%r9
	slbr	%r6,%r9
	larl	%r6,.
	lrv	%r6,4095(%r5,%r10)
	lrvh	%r6,4095(%r5,%r10)
	strv	%r6,4095(%r5,%r10)
	strvh	%r6,4095(%r5,%r10)
	ml	%r6,4095(%r5,%r10)
	dl	%r6,4095(%r5,%r10)
	alc	%r6,4095(%r5,%r10)
	slb	%r6,4095(%r5,%r10)
	rll	%r6,%r9,4095(%r5)
