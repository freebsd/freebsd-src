	.h8300s
	.text
h8300s_branches:
	bsr h8300s_branches:8
	bsr h8300s_branches:16
	jmp h8300s_branches
	jmp @er0
	jmp @@16:8
	jsr h8300s_branches
	jsr @er0
	jsr @@16:8

