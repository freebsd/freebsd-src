	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	stb  r0,@r2-
	jmp r13	

