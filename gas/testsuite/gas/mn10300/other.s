	.text
	clr d2
	inc d1
	inc a2
	inc4 a3
	jmp (a2)
	jmp 256
	jmp 131071
	call 256,[a2,a3],9
	call 131071,[a2,a3],32
	calls (a2)
	calls 256
	calls 131071
	ret [a2,a3],7
	retf [a2,a3],5
	rets
	rti
	trap
	nop
	rtm
