
	.text
	.global jumps
jumps:
	jarl jumps,r5
	jmp [r5]
	jr jumps
	
