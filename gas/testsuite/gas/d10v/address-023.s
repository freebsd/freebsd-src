	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ldub r0,@sp+
	jmp r13	

