	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	st2w r0,@+r2
	jmp r13	

