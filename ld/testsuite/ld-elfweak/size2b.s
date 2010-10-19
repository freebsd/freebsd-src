	.text
	.weak _start
	.weak __start
	.type _start, "function"
	.type __start, "function"
_start:
__start:
	.byte 0
	.byte 0
	.size _start, 2
	.size __start, 2

	.weak foo
	.type foo, "function"
foo:
	.byte 0
	.byte 0
	.size foo, 2
