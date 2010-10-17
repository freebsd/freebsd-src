	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	st   r0,@sp-
	jmp r13	

