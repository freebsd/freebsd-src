	;; Test pc relative relocation

	.text	
	.global _start
_start:
	nop
	brf0f.s foo 
	jmp r13	




