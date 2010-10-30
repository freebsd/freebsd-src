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
	.globl data
	.section	.data,"aw",%progbits
	.p2align 4
	.type	data,%object
	.size	data,4096
data:
	.zero	4096
	.globl tdata
	.section	.tdata,"awT",%progbits
	.p2align 4
	.type	tdata,%object
	.size	tdata,4096
tdata:
	.zero	4096
