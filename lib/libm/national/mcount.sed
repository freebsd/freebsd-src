s/.word	0x0.*$/&\
	.data\
1:\
	.long	0\
	.text\
	addr	1b,r0\
	jsb	mcount/
