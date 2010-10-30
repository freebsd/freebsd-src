	.text
	.cfi_startproc simple
	.cfi_personality 3, my_personality_v0
	.cfi_lsda 12, 0xdeadbeef
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality 3, my_personality_v0
	.cfi_lsda 12, 0xdeadbeef
	.cfi_personality 0xff
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality 3, my_personality_v0
	.cfi_lsda 12, 0xbeefdead
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality (0x1b), my_personality_v1
	.cfi_lsda 27, 1f
	.long 0
	.cfi_endproc

	.cfi_startproc simple
	.cfi_personality (0x1b), my_personality_v1
	.cfi_lsda 27, 2f
	.long 0
	.cfi_endproc

my_personality_v0:
	.long 0
my_personality_v1:
	.long 0
1:
	.long 0
2:
	.long 0
