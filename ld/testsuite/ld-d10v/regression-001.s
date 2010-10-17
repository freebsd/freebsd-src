	.section .data
	;;
	;; The next line caused an earlier ld to core dump.
	.global  .data
foo:
	.space 0x0064

	.section .text
	.global _test
	.global _start
_test:	
	ldi r0,foo
_start:
	nop
