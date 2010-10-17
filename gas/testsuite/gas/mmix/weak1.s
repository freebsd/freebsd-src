	.weak foo
foo:
	POP 1,0
	.global main
main:
	PUSHJ $15,foo
	.quad foo
