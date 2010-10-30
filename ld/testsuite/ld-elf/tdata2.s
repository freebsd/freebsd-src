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
	.globl tdata
	.section	.tdata,"awT",%progbits
	.type	tdata,%object
	.size	tdata,1
tdata:
	.byte 0
