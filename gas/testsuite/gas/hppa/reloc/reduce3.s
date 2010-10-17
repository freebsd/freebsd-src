	.data
	.align 8
blah:
	; .double 0e+00
	.word 0 ; = 0x0
	.word 0 ; = 0x0
	.EXPORT foo,DATA
	.align 8
foo:
	; .double 0e+00
	.word 0 ; = 0x0
	.word 0 ; = 0x0
	.EXPORT yabba,DATA
	.align 4
yabba:
	.word 1
	.code

	.align 4
	.EXPORT bar,CODE
	.EXPORT bar,ENTRY,PRIV_LEV=3,RTNVAL=GR
bar:
	.PROC
	.CALLINFO FRAME=64,NO_CALLS,SAVE_SP,ENTRY_GR=3
	.ENTRY
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,64(%r30)
	addil L'yabba-$global$,%r27
	ldo R'yabba-$global$(%r1),%r19
	ldi 2,%r20
	stw %r20,0(%r19)
L$0001:
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
