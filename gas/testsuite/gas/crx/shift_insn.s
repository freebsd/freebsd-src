# Shift instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global sllb
sllb:
sllb $7 , r1
sllb r2 , r3

	.global srlb
srlb:
srlb $0x5 , r4
srlb r5 , r6

	.global srab
srab:
srab $04 , r7
srab r8 , r9

	.global sllw
sllw:
sllw $15 , r10
sllw r11 , r12

	.global srlw
srlw:
srlw $0xe , r13
srlw r14 , r15

	.global sraw
sraw:
sraw $015 , ra
sraw sp , r1

	.global slld
slld:
slld $31 , r2
slld r3 , r4

	.global srld
srld:
srld $0x1f , r5
srld r6 , r7

	.global srad
srad:
srad $022 , r8
srad r9 , r10

