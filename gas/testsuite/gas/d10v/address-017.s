	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ld   r0,@-r2
	jmp r13	

