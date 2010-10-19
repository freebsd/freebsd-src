# 'Branch if Equal to 0' instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global beq0b
beq0b:
beq0b r10 , *+22

	.global bne0b
bne0b:
bne0b r11 , *+0x20

	.global beq0w
beq0w:
beq0w r12 , *+2

	.global bne0w
bne0w:
bne0w r13 , *+040

	.global beq0d
beq0d:
beq0d ra , *+32

	.global bne0d
bne0d:
bne0d sp , *+16



