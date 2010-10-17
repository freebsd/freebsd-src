	;; Test 18 bit pc rel relocation

	.text	
	.global _start
_start:
	bra.l foo 
	jmp r13	




