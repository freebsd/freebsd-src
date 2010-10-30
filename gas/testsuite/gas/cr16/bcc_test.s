        .text
        .global main
main:
	###################
	# bcc disp9/disp17/disp25
	###################
	# bcc disp9
	###################
	beq	*+0x022
	bne	*+0x032
	bcc	*+0x044
	bcc	*+0x054
	bhi	*+0x066
	blt	*+0x076
	bgt	*+0x088
	bfs	*+0x09a
	bfc	*+0x0aa
	blo	*+0x1bc
	bhi	*+0x1cc
	blt	*+0x1d6
	bge	*+0x1e6
	br	*+0x0f6
	###################
	# bcc disp17
	###################
	beq	*+0x112
	beq	*+0x1f12
	beq	*+0x0f22
	bne	*+0x0f34
	bcc	*+0x0f44
	bcc	*+0x0f56
	bhi	*+0x0f66
	blt	*+0x0f78
	bgt	*+0x0f88
	bfs	*+0x0f9a
	bfc	*+0x0faa
	blo	*+0x1fbc
	bhi	*+0x1fcc
	blt	*+0x1fda
	bge	*+0x1fea
	br	*+0xfffa
	###################
	# bcc disp25
	###################
	beq	*+0xff1f12
	beq	*+0xaa0f22
	bne	*+0xbb0f34
	bcc	*+0xcc0f44
	bcc	*+0xdd0f56
	bhi	*+0x990f66
	blt	*+0x880f78
	bgt	*+0x770f88
	bfs	*+0x660f9a
	bfc	*+0x550faa
	blo	*+0x441fbc
	bhi	*+0x331fcc
	blt	*+0x221fde
	bge	*+0x111fee
	br	*+0x0ffffe
