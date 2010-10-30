	.globl main
	.globl start
	.globl _start
	.globl __start
	.text
main:
start:
_start:
__start:
	.byte 0
	.globl tbss
	.section	.tbss,"awT",%nobits
	.type	tbss,%object
	.size	tbss,1
tbss:
	.zero	1
