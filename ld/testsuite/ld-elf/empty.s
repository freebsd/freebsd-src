	.section .bss
bar:
	.text
	.type start,"function"
	.global start
start:
	.type _start,"function"
	.global _start
_start:
	.type __start,"function"
	.global __start
__start:
	.type main,"function"
	.global main
main:
	.long 0
