	.text
	.cfi_startproc simple
	.cfi_def_cfa 0, 16
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality 3, my_personality_v1
	.long 0
	.cfi_def_cfa 0, 16
	.cfi_endproc

	.cfi_startproc simple
	.cfi_def_cfa 0, 16
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality 3, my_personality_v1
	.cfi_lsda 12, 0xdeadbeef
	.long 0
	.cfi_def_cfa 0, 16
	.cfi_endproc

	.globl my_personality_v1
my_personality_v1:
	.long 0
