# JCond/SCond instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global jeq
jeq:
jeq r1

	.global jne
jne:
jne r2

	.global jcs
jcs:
jcs r3

	.global jcc
jcc:
jcc r4

	.global jhi
jhi:
jhi r5

	.global jls
jls:
jls r6

	.global jgt
jgt:
jgt r7

	.global jle
jle:
jle r8

	.global jfs
jfs:
jfs r9

	.global jfc
jfc:
jfc r10

	.global jlo
jlo:
jlo r11

	.global jhs
jhs:
jhs r12

	.global jlt
jlt:
jlt r13

	.global jge
jge:
jge ra

	.global jump
jump:
jump sp

	.global seq
seq:
seq r1

	.global sne
sne:
sne r2

	.global scs
scs:
scs r3

	.global scc
scc:
scc r4

	.global shi
shi:
shi r5

	.global sls
sls:
sls r6

	.global sgt
sgt:
sgt r7

	.global sle
sle:
sle r8

	.global sfs
sfs:
sfs r9

	.global sfc
sfc:
sfc r10

	.global slo
slo:
slo r11

	.global shs
shs:
shs r12

	.global slt
slt:
slt r13

	.global sge
sge:
sge ra
