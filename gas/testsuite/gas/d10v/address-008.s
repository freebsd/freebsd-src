	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ldb  r0,@+r2
	jmp r13	

