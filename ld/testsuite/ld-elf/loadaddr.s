	.text
	.globl main
	.globl start
	.globl _start
	.globl __start
main:
start:
_start:
__start:
	.byte 0,0,0,0,0,0,0,0
	.byte 0,0,0,0,0,0,0,0
	.section .bar,"ax","progbits"
	.byte 0,0,0,0,0,0,0,0
	.byte 0,0,0,0,0,0,0,0
	.section .foo,"aw","progbits"
	.byte 0,0,0,0,0,0,0,0
	.byte 0,0,0,0,0,0,0,0
	.data
	.byte 0,0,0,0,0,0,0,0
	.byte 0,0,0,0,0,0,0,0
