	;; Test unsupported indirect addressing

	.text	
	.global main
main:
	stb  r0,@sp+
	jmp r13	

