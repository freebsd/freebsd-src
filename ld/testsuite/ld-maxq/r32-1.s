; Test the intersegment relocation
; Inderpreetb@noida.hcltech.com

.global _start
.extern _main
_start:
	call _main
	nop
	nop
	nop
	nop
.global _exit
_exit:
	nop
	nop
	nop
.global _abort	
_abort:
	nop	
	nop
