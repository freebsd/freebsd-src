	.code
	.align 4
s:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r30,12(%r4)
	ldil L'L$0007,%r19
	ldo R'L$0007(%r19),%r19
	comib,>= 0,%r26,L$0002
	stw %r19,8(%r4)
L$0003:
L$0002:
	b L$0001
	ldo 1(%r0),%r28
L$0007:
	ldil L'L$0002,%r19
	ldo R'L$0002(%r19),%r19
	comb,= %r29,%r19,L$0002
	ldo -8(%r4),%r4
	.EXIT
	.PROCEND
