	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	st2w r0,@sp-
	jmp r13	

