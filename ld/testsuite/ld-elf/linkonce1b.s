	.globl main
	.globl start
	.globl _start
	.globl __start
	.text
main:
start:
_start:
__start:
	.long	0

        .section .gnu.linkonce.d.dummy,"aw"
        .long    0
foo:
        .long    0
 	.section        .debug_frame,"",%progbits
        .long    foo
