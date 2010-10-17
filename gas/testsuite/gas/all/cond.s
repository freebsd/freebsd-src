	.if	0
	.if	1
	.endc
	.long	0
	.if	0
	.long	1
	.endc
	.else
	.if	1
	.endc
	.long	2
	.if	0
	.long	3
	.else
	.long	4
	.endc
	.endc

	.if	0
	.long	5
	.elseif	1
	.if	0
	.long	6
	.elseif	1
	.long	7
	.endif
	.elseif	1
	.long	8
	.else
	.long	9
	.endif
	.p2align 5,0
