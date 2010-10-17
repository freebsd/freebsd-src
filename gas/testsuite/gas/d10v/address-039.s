	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ld   r0,@-sp
	jmp r13	

