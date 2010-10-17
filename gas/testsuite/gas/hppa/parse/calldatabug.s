	.code
	.align 4
LC$0000:
	.STRING "%d %lf %d\x0a\x00"
	.align 4
	.EXPORT error__3AAAiidi
	.EXPORT error__3AAAiidi,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=FR,ARGW4=FU,RTNVAL=GR
error__3AAAiidi:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r9,8(%r4)
	stw %r8,12(%r4)
	stw %r7,16(%r4)
	stw %r6,20(%r4)
	stw %r5,24(%r4)
	copy %r26,%r5
	ldo -8(%r0),%r6
	ldo -32(%r4),%r19
	add %r19,%r6,%r7
	stw %r25,0(%r7)
	ldo -12(%r0),%r8
	ldo -32(%r4),%r19
	add %r19,%r8,%r9
	stw %r24,0(%r9)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -24(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -28(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	stw %r22,-52(%r30)
	ldil L'LC$0000,%r26
	ldo R'LC$0000(%r26),%r26
	ldw 0(%r19),%r25
	fldds 0(%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl printf,%r2
	nop
	bl,n L$0002,%r0
	bl,n L$0001,%r0
L$0002:
L$0001:
	ldw 8(%r4),%r9
	ldw 12(%r4),%r8
	ldw 16(%r4),%r7
	ldw 20(%r4),%r6
	ldw 24(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT ok__3AAAidi
	.EXPORT ok__3AAAidi,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU,RTNVAL=GR
ok__3AAAidi:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r9,8(%r4)
	stw %r8,12(%r4)
	stw %r7,16(%r4)
	stw %r6,20(%r4)
	stw %r5,24(%r4)
	copy %r26,%r5
	ldo -8(%r0),%r6
	ldo -32(%r4),%r19
	add %r19,%r6,%r7
	stw %r25,0(%r7)
	ldo -16(%r0),%r8
	ldo -32(%r4),%r19
	add %r19,%r8,%r9
	fstds %fr7,0(%r9)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -16(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -20(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	stw %r22,-52(%r30)
	ldil L'LC$0000,%r26
	ldo R'LC$0000(%r26),%r26
	ldw 0(%r19),%r25
	fldds 0(%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl printf,%r2
	nop
	bl,n L$0004,%r0
	bl,n L$0003,%r0
L$0004:
L$0003:
	ldw 8(%r4),%r9
	ldw 12(%r4),%r8
	ldw 16(%r4),%r7
	ldw 20(%r4),%r6
	ldw 24(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT __main,CODE
	.align 8
LC$0001:
	; .double 5.50000000000000000000e+00
	.word 1075183616 ; = 0x40160000
	.word 0 ; = 0x0
	.align 4
	.EXPORT main
	.EXPORT main,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	.CALL 
	bl __main,%r2
	nop
	ldo -24(%r0),%r19
	ldo -32(%r30),%r20
	add %r20,%r19,%r19
	ldil L'LC$0001,%r20
	ldo R'LC$0001(%r20),%r21
	ldw 0(%r21),%r22
	ldw 4(%r21),%r23
	stw %r22,0(%r19)
	stw %r23,4(%r19)
	ldo 3(%r0),%r19
	stw %r19,-60(%r30)
	ldo 8(%r4),%r26
	ldo 1(%r0),%r25
	ldo 4(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl error__3AAAiidi,%r2
	nop
	ldo 3(%r0),%r19
	stw %r19,-52(%r30)
	ldo 8(%r4),%r26
	ldo 1(%r0),%r25
	ldil L'LC$0001,%r19
	ldo R'LC$0001(%r19),%r20
	fldds 0(%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl ok__3AAAidi,%r2
	nop
	copy %r0,%r28
	bl,n L$0005,%r0
	bl,n L$0005,%r0
L$0005:
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND

