	.macro M arg1
	.ascii	"\arg1"
	.endm

	.data
foo:
	M "\\\"foo\\\""

	.balign 2

	M "bar"

	.balign 2

	M baz


