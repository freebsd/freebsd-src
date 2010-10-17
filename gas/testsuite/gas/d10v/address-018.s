	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ld2w r0,@-r2
	jmp r13	

