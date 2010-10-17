	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	ldb  r0,@+sp
	jmp r13	

