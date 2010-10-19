	.text
	.global _start
	.global __start
	.type _start, "function"
	.type __start, "function"
_start:
__start:
	.byte 0
	.size _start, 1
	.size __start, 1

	.weak foo
	.type foo, "function"
foo:
	.byte 0
	.size foo, 1
