	.h8300h
	.text
h8300h_branches:
	bsr h8300h_branches:8
	bsr h8300h_branches:16
	jmp h8300h_branches
	jmp @er0
	jmp @@16:8
	jsr h8300h_branches
	jsr @er0
	jsr @@16:8

