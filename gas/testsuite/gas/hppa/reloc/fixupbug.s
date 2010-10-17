	.code
	b,n	$$foo
	nop
	nop

	.SPACE $TEXT$
	.SUBSPA $MILLICODE$
$$foo:
	nop
