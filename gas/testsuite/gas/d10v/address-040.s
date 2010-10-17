	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ld2w r0,@-sp
	jmp r13	

