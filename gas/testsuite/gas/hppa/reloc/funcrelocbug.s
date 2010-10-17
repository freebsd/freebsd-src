	.code
	.align 4
	.EXPORT g,CODE
	.EXPORT g,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,RTNVAL=GR
g:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.ENTRY
	stw %r2,-20(%r30)
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,128(%r30)
	stw %r26,-36(%r3)
	stw %r25,-40(%r3)
	stw %r24,-44(%r3)
	ldw -36(%r3),%r26
	ldw -40(%r3),%r25
	ldw -44(%r3),%r19
	copy %r19,%r22
	.CALL	ARGW0=GR
	bl $$dyncall,%r31
	copy %r31,%r2
	copy %r28,%r19
	comiclr,<> 0,%r19,%r0
	bl,n L$0002,%r0
	ldw -36(%r3),%r28
	bl,n L$0001,%r0
	bl,n L$0003,%r0
L$0002:
	ldw -40(%r3),%r28
	bl,n L$0001,%r0
L$0003:
L$0001:
	ldw -20(%r3),%r2
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
	.align 4
f2___4:
	.PROC
	.CALLINFO FRAME=64,NO_CALLS,SAVE_SP,ENTRY_GR=3
	.ENTRY
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,64(%r30)
	stw %r29,8(%r3)
	stw %r26,-36(%r3)
	stw %r25,-40(%r3)
	ldw -36(%r3),%r19
	ldw -40(%r3),%r20
	comclr,>= %r20,%r19,%r19
	ldi 1,%r19
	copy %r19,%r28
	bl,n L$0005,%r0
L$0005:
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
	.IMPORT abort,CODE
	.data

	.align 4
L$TRAMP0000:
	ldw	36(%r22),%r21
	bb,>=,n	%r21,30,.+16
	depi	0,31,2,%r21
	ldw	4(%r21),%r19
	ldw	0(%r21),%r21
	ldsid	(%r21),%r1
	mtsp	%r1,%sr0
	be	0(%sr0,%r21)
	ldw	40(%r22),%r29
	.word	0
	.word	0
	.code

	.align 4
	.EXPORT f,CODE
	.EXPORT f,ENTRY,PRIV_LEV=3,RTNVAL=GR
f:
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.ENTRY
	stw %r2,-20(%r30)
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,192(%r30)
	ldo 16(%r3),%r19
	addil L'L$TRAMP0000-$global$,%r27
	ldo R'L$TRAMP0000-$global$(%r1),%r22
	ldo 40(%r0),%r20
	ldws,ma 4(%r22),%r21
	addib,>= -4,%r20,.-4
	stws,ma %r21,4(%r19)
	ldil L'f2___4,%r20
	ldo R'f2___4(%r20),%r19
	stw %r19,52(%r3)
	ldo 8(%r3),%r19
	stw %r19,56(%r3)
	ldo 16(%r3),%r19
	ldo 48(%r3),%r20
	fdc %r0(%r19)
	fdc %r0(%r20)
	sync
	ldo 32(%r19),%r22
	mfsp %sr0,%r21
	ldsid (%r19),%r20
	mtsp %r20,%sr0
	fic %r0(%sr0,%r19)
	fic %r0(%sr0,%r22)
	sync
	mtsp %r21,%sr0
	nop
	nop
	nop
	nop
	nop
	nop
	ldo 16(%r3),%r19
	ldi 1,%r26
	ldi 2,%r25
	copy %r19,%r24
	.CALL ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO
	bl g,%r2
	nop
	copy %r28,%r19
	comiclr,<> 2,%r19,%r0
	bl,n L$0006,%r0
	.CALL ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO
	bl abort,%r2
	nop
L$0006:
L$0004:
	ldw -20(%r3),%r2
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
	.IMPORT __main,CODE
	.IMPORT exit,CODE
	.align 4
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.ENTRY
	stw %r2,-20(%r30)
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,128(%r30)
	.CALL ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO
	bl __main,%r2
	nop
	.CALL ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO
	bl f,%r2
	nop
	copy %r0,%r26
	.CALL ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO
	bl exit,%r2
	nop
L$0007:
	ldw -20(%r3),%r2
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
