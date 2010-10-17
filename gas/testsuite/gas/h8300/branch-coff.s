	.text
h8300_branches:
	bsr h8300_branches
	jmp h8300_branches
	jmp @r0
	jmp @@16:8
	jsr h8300_branches
	jsr @r0
	jsr @@16:8

