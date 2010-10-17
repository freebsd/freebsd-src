	.section zpage
vector:
	.word h8300_branches
	.text
h8300_branches:
	bsr h8300_branches
	jmp h8300_branches
	jmp @r0
	jmp @@vector:8
	jsr h8300_branches
	jsr @r0
	jsr @@vector:8

