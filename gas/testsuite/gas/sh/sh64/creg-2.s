! Test recognition of predefined control register names specified as crN
! syntax, lower and upper case.

	.mode SHmedia
	.text
start:
	getcon cr0,r21
	getcon cr13,r21
	getcon CR62,r22
	getcon cr8,r21
	getcon CR4,r21
	putcon r19,cr11
	putcon r38,CR5
	putcon r21,CR1
