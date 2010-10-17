	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	st   r0,@+r2
	jmp r13	

