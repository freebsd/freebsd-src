# Conditional move instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global cmoveqd
cmoveqd:
cmoveqd r0 , r1

	.global cmovned
cmovned:
cmovned r2 , r3

	.global cmovcsd
cmovcsd:
cmovcsd r4 , r5

	.global cmovccd
cmovccd:
cmovccd r6 , r7

	.global cmovhid
cmovhid:
cmovhid r8 , r9

	.global cmovlsd
cmovlsd:
cmovlsd r10 , r11

	.global cmovgtd
cmovgtd:
cmovgtd r12 , r13

	.global cmovled
cmovled:
cmovled r14 , sp

	.global cmovfsd
cmovfsd:
cmovfsd r15 , ra

	.global cmovfcd
cmovfcd:
cmovfcd sp , ra

	.global cmovlod
cmovlod:
cmovlod r15 , r0

	.global cmovhsd
cmovhsd:
cmovhsd r2 , r3

	.global cmovltd
cmovltd:
cmovltd r7 , r5

	.global cmovged
cmovged:
cmovged r3 , r4
