	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ldub r0,@-r2
	jmp r13	

