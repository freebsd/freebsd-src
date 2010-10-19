; Addend check testcases 
; inderpreetb@noida.hcltech.com
.global _main
_main:
_buf0:
	.long 0x5678
_buf1:
	.long 0x1234
_start:
	nop
	nop
	move A[0], _buf1+2
	move A[0], _buf1-2
	call _buf0+8
	call _buf1+2

