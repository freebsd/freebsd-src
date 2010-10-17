	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
	.code
	.align 4
	.EXPORT alloc_type,CODE
	.EXPORT alloc_type,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
alloc_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r7,32(%r4)
	stw %r6,36(%r4)
	stw %r5,40(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0002,%r0
	nop
	ldo 52(%r0),%r26
	.CALL ARGW0=GR
	bl xmalloc,%r2
	nop
	copy %r28,%r7
	bl,n L$0003,%r0
L$0002: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 120(%r19),%r20
	stw %r20,8(%r4)
	ldw 8(%r4),%r19
	stw %r19,12(%r4)
	ldo 52(%r0),%r19
	stw %r19,16(%r4)
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 16(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0004,%r0
	nop
	ldw 12(%r4),%r26
	ldw 16(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0005,%r0
L$0004: 
	copy %r0,%r19
L$0005: 
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 12(%r20),%r21
	ldw 16(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 8(%r4),%r19
	stw %r19,20(%r4)
	ldw 20(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,24(%r4)
	ldw 20(%r4),%r19
	ldw 12(%r19),%r20
	ldw 24(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0006,%r0
	nop
	ldw 20(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0006: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 20(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0007,%r0
	nop
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0008,%r0
L$0007: 
	copy %r0,%r19
L$0008: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 24(%r4),%r7
L$0003: 
	copy %r7,%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	stw %r0,0(%r7)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,12(%r7)
	ldo -1(%r0),%r19
	stw %r19,44(%r7)
	copy %r7,%r28
	bl,n L$0001,%r0
L$0001: 
	ldw 32(%r4),%r7
	ldw 36(%r4),%r6
	ldw 40(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT make_pointer_type,CODE
	.EXPORT make_pointer_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
make_pointer_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r9,16(%r4)
	stw %r8,20(%r4)
	stw %r7,24(%r4)
	stw %r6,28(%r4)
	stw %r5,32(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 20(%r19),%r9
	comiclr,<> 0,%r9,%r0
	bl L$0010,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0011,%r0
	nop
	copy %r9,%r28
	bl,n L$0009,%r0
	bl,n L$0012,%r0
L$0011: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0013,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
	copy %r9,%r28
	bl,n L$0009,%r0
L$0013: 
L$0012: 
L$0010: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0015,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0014,%r0
	nop
	bl,n L$0015,%r0
L$0015: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	copy %r28,%r9
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0016,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
L$0016: 
	bl,n L$0017,%r0
L$0014: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r9
	ldw 12(%r9),%r19
	stw %r19,8(%r4)
	copy %r9,%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	ldw 8(%r4),%r19
	stw %r19,12(%r9)
L$0017: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,16(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,20(%r19)
	ldo 4(%r0),%r19
	stw %r19,8(%r9)
	ldo 1(%r0),%r19
	stw %r19,0(%r9)
	ldh 32(%r9),%r19
	copy %r19,%r20
	depi -1,31,1,%r20
	sth %r20,32(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 20(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0018,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,20(%r19)
L$0018: 
	copy %r9,%r28
	bl,n L$0009,%r0
L$0009: 
	ldw 16(%r4),%r9
	ldw 20(%r4),%r8
	ldw 24(%r4),%r7
	ldw 28(%r4),%r6
	ldw 32(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT lookup_pointer_type,CODE
	.EXPORT lookup_pointer_type,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_pointer_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,8(%r4)
	stw %r5,12(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl make_pointer_type,%r2
	nop
	bl,n L$0019,%r0
L$0019: 
	ldw 8(%r4),%r6
	ldw 12(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT make_reference_type,CODE
	.EXPORT make_reference_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
make_reference_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r9,16(%r4)
	stw %r8,20(%r4)
	stw %r7,24(%r4)
	stw %r6,28(%r4)
	stw %r5,32(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 24(%r19),%r9
	comiclr,<> 0,%r9,%r0
	bl L$0021,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0022,%r0
	nop
	copy %r9,%r28
	bl,n L$0020,%r0
	bl,n L$0023,%r0
L$0022: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0024,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
	copy %r9,%r28
	bl,n L$0020,%r0
L$0024: 
L$0023: 
L$0021: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0026,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0025,%r0
	nop
	bl,n L$0026,%r0
L$0026: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	copy %r28,%r9
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0027,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
L$0027: 
	bl,n L$0028,%r0
L$0025: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r9
	ldw 12(%r9),%r19
	stw %r19,8(%r4)
	copy %r9,%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	ldw 8(%r4),%r19
	stw %r19,12(%r9)
L$0028: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,16(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,24(%r19)
	ldo 4(%r0),%r19
	stw %r19,8(%r9)
	ldo 16(%r0),%r19
	stw %r19,0(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 24(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0029,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,24(%r19)
L$0029: 
	copy %r9,%r28
	bl,n L$0020,%r0
L$0020: 
	ldw 16(%r4),%r9
	ldw 20(%r4),%r8
	ldw 24(%r4),%r7
	ldw 28(%r4),%r6
	ldw 32(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT lookup_reference_type,CODE
	.EXPORT lookup_reference_type,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_reference_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,8(%r4)
	stw %r5,12(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl make_reference_type,%r2
	nop
	bl,n L$0030,%r0
L$0030: 
	ldw 8(%r4),%r6
	ldw 12(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT make_function_type,CODE
	.EXPORT make_function_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
make_function_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r9,16(%r4)
	stw %r8,20(%r4)
	stw %r7,24(%r4)
	stw %r6,28(%r4)
	stw %r5,32(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 28(%r19),%r9
	comiclr,<> 0,%r9,%r0
	bl L$0032,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0033,%r0
	nop
	copy %r9,%r28
	bl,n L$0031,%r0
	bl,n L$0034,%r0
L$0033: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0035,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
	copy %r9,%r28
	bl,n L$0031,%r0
L$0035: 
L$0034: 
L$0032: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0037,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0036,%r0
	nop
	bl,n L$0037,%r0
L$0037: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	copy %r28,%r9
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0038,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,0(%r19)
L$0038: 
	bl,n L$0039,%r0
L$0036: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r9
	ldw 12(%r9),%r19
	stw %r19,8(%r4)
	copy %r9,%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	ldw 8(%r4),%r19
	stw %r19,12(%r9)
L$0039: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,16(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,28(%r19)
	ldo 1(%r0),%r19
	stw %r19,8(%r9)
	ldo 6(%r0),%r19
	stw %r19,0(%r9)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 28(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0040,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	stw %r9,28(%r19)
L$0040: 
	copy %r9,%r28
	bl,n L$0031,%r0
L$0031: 
	ldw 16(%r4),%r9
	ldw 20(%r4),%r8
	ldw 24(%r4),%r7
	ldw 28(%r4),%r6
	ldw 32(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT lookup_function_type,CODE
	.EXPORT lookup_function_type,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_function_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,8(%r4)
	stw %r5,12(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl make_function_type,%r2
	nop
	bl,n L$0041,%r0
L$0041: 
	ldw 8(%r4),%r6
	ldw 12(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT smash_to_member_type,CODE
	.align 4
	.EXPORT lookup_member_type,CODE
	.EXPORT lookup_member_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
lookup_member_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r8,8(%r4)
	stw %r7,12(%r4)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo 24(%r4),%r1
	fstds,ma %fr12,8(%r1)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	stw %r28,-16(%r30)
	fldws -16(%r30),%fr12
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	fstws %fr12,-16(%r30)
	ldw -16(%r30),%r26
	ldw 0(%r19),%r25
	ldw 0(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl smash_to_member_type,%r2
	nop
	fstws %fr12,-16(%r30)
	ldw -16(%r30),%r28
	bl,n L$0042,%r0
L$0042: 
	ldw 8(%r4),%r8
	ldw 12(%r4),%r7
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 24(%r4),%r1
	fldds,ma 8(%r1),%fr12
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT allocate_stub_method,CODE
	.EXPORT allocate_stub_method,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
allocate_stub_method: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	stw %r28,8(%r4)
	ldw 8(%r4),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,16(%r19)
	ldw 8(%r4),%r19
	ldo 4(%r0),%r20
	sth %r20,32(%r19)
	ldw 8(%r4),%r19
	ldo 15(%r0),%r20
	stw %r20,0(%r19)
	ldw 8(%r4),%r19
	ldo 1(%r0),%r20
	stw %r20,8(%r19)
	ldw 8(%r4),%r28
	bl,n L$0043,%r0
L$0043: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT builtin_type_int,DATA
	.align 4
	.EXPORT create_array_type,CODE
	.EXPORT create_array_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
create_array_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r10,56(%r4)
	stw %r9,60(%r4)
	stw %r8,64(%r4)
	stw %r7,68(%r4)
	stw %r6,72(%r4)
	stw %r5,76(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	stw %r28,8(%r4)
	ldw 8(%r4),%r19
	ldo 2(%r0),%r20
	stw %r20,0(%r19)
	ldw 8(%r4),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,16(%r19)
	ldw 8(%r4),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 0(%r20),%r20
	ldw 8(%r21),%r21
	stw %r20,-16(%r30)
	fldws -16(%r30),%fr5
	stw %r21,-16(%r30)
	fldws -16(%r30),%fr5R
	xmpyu %fr5,%fr5R,%fr4
	fstws %fr4R,-16(%r30)
	ldw -16(%r30),%r24
	stw %r24,8(%r19)
	ldw 8(%r4),%r19
	ldo 1(%r0),%r20
	sth %r20,34(%r19)
	ldw 8(%r4),%r9
	ldw 8(%r4),%r19
	ldw 12(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0050,%r0
	nop
	ldw 8(%r4),%r19
	ldw 12(%r19),%r20
	ldo 120(%r20),%r19
	stw %r19,16(%r4)
	ldw 16(%r4),%r19
	stw %r19,20(%r4)
	ldo 16(%r0),%r19
	stw %r19,24(%r4)
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 24(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0045,%r0
	nop
	ldw 20(%r4),%r26
	ldw 24(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0046,%r0
L$0045: 
	copy %r0,%r19
L$0046: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r20),%r21
	ldw 24(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 16(%r4),%r19
	stw %r19,28(%r4)
	ldw 28(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,32(%r4)
	ldw 28(%r4),%r19
	ldw 12(%r19),%r20
	ldw 32(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0047,%r0
	nop
	ldw 28(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0047: 
	ldw 28(%r4),%r19
	ldw 28(%r4),%r20
	ldw 28(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 28(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 28(%r4),%r19
	ldw 28(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 28(%r4),%r20
	ldw 28(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0048,%r0
	nop
	ldw 28(%r4),%r19
	ldw 28(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0049,%r0
L$0048: 
	copy %r0,%r19
L$0049: 
	ldw 28(%r4),%r19
	ldw 28(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 32(%r4),%r10
	bl,n L$0051,%r0
L$0050: 
	ldo 16(%r0),%r26
	.CALL ARGW0=GR
	bl xmalloc,%r2
	nop
	copy %r28,%r10
L$0051: 
	stw %r10,36(%r9)
	ldw 8(%r4),%r19
	ldw 12(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	stw %r28,12(%r4)
	ldw 12(%r4),%r19
	ldo 11(%r0),%r20
	stw %r20,0(%r19)
	ldw 12(%r4),%r19
	addil L'builtin_type_int-$global$,%r27
	ldw R'builtin_type_int-$global$(%r1),%r20
	stw %r20,16(%r19)
	ldw 12(%r4),%r19
	ldo 4(%r0),%r20
	stw %r20,8(%r19)
	ldw 12(%r4),%r19
	ldo 2(%r0),%r20
	sth %r20,34(%r19)
	ldw 12(%r4),%r9
	ldw 12(%r4),%r19
	ldw 12(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0057,%r0
	nop
	ldw 12(%r4),%r19
	ldw 12(%r19),%r20
	ldo 120(%r20),%r19
	stw %r19,36(%r4)
	ldw 36(%r4),%r19
	stw %r19,40(%r4)
	ldo 32(%r0),%r19
	stw %r19,44(%r4)
	ldw 40(%r4),%r19
	ldw 40(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 44(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0052,%r0
	nop
	ldw 40(%r4),%r26
	ldw 44(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0053,%r0
L$0052: 
	copy %r0,%r19
L$0053: 
	ldw 40(%r4),%r19
	ldw 40(%r4),%r20
	ldw 12(%r20),%r21
	ldw 44(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 36(%r4),%r19
	stw %r19,48(%r4)
	ldw 48(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,52(%r4)
	ldw 48(%r4),%r19
	ldw 12(%r19),%r20
	ldw 52(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0054,%r0
	nop
	ldw 48(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0054: 
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 48(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 48(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 48(%r4),%r20
	ldw 48(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0055,%r0
	nop
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0056,%r0
L$0055: 
	copy %r0,%r19
L$0056: 
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 52(%r4),%r10
	bl,n L$0058,%r0
L$0057: 
	ldo 32(%r0),%r26
	.CALL ARGW0=GR
	bl xmalloc,%r2
	nop
	copy %r28,%r10
L$0058: 
	stw %r10,36(%r9)
	ldw 12(%r4),%r19
	ldw 36(%r19),%r20
	stw %r0,0(%r20)
	ldw 12(%r4),%r19
	ldo 16(%r0),%r20
	ldw 36(%r19),%r21
	add %r20,%r21,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldo -1(%r20),%r21
	stw %r21,0(%r19)
	ldw 12(%r4),%r20
	ldw 36(%r20),%r19
	addil L'builtin_type_int-$global$,%r27
	ldw R'builtin_type_int-$global$(%r1),%r20
	stw %r20,8(%r19)
	ldw 12(%r4),%r19
	ldo 16(%r0),%r20
	ldw 36(%r19),%r21
	add %r20,%r21,%r19
	addil L'builtin_type_int-$global$,%r27
	ldw R'builtin_type_int-$global$(%r1),%r20
	stw %r20,8(%r19)
	ldw 8(%r4),%r19
	ldw 36(%r19),%r20
	ldw 12(%r4),%r19
	stw %r19,8(%r20)
	ldw 8(%r4),%r19
	ldo -1(%r0),%r20
	stw %r20,44(%r19)
	ldw 8(%r4),%r28
	bl,n L$0044,%r0
L$0044: 
	ldw 56(%r4),%r10
	ldw 60(%r4),%r9
	ldw 64(%r4),%r8
	ldw 68(%r4),%r7
	ldw 72(%r4),%r6
	ldw 76(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT smash_to_member_type,CODE
	.EXPORT smash_to_member_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR
smash_to_member_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r10,16(%r4)
	stw %r9,20(%r4)
	stw %r8,24(%r4)
	stw %r7,28(%r4)
	stw %r6,32(%r4)
	stw %r5,36(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	stw %r20,8(%r4)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	stw %r20,12(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -12(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,16(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,40(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 1(%r0),%r20
	stw %r20,8(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 14(%r0),%r20
	stw %r20,0(%r19)
L$0059: 
	ldw 16(%r4),%r10
	ldw 20(%r4),%r9
	ldw 24(%r4),%r8
	ldw 28(%r4),%r7
	ldw 32(%r4),%r6
	ldw 36(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT smash_to_method_type,CODE
	.EXPORT smash_to_method_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
smash_to_method_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r12,16(%r4)
	stw %r11,20(%r4)
	stw %r10,24(%r4)
	stw %r9,28(%r4)
	stw %r8,32(%r4)
	stw %r7,36(%r4)
	stw %r6,40(%r4)
	stw %r5,44(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -16(%r0),%r11
	ldo -32(%r4),%r19
	add %r19,%r11,%r12
	stw %r23,0(%r12)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	stw %r20,8(%r4)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	stw %r20,12(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -12(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,16(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,40(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -16(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,48(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 1(%r0),%r20
	stw %r20,8(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 15(%r0),%r20
	stw %r20,0(%r19)
L$0060: 
	ldw 16(%r4),%r12
	ldw 20(%r4),%r11
	ldw 24(%r4),%r10
	ldw 28(%r4),%r9
	ldw 32(%r4),%r8
	ldw 36(%r4),%r7
	ldw 40(%r4),%r6
	ldw 44(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT strncmp,CODE
	.align 4
LC$0000: 
	.STRING "struct \x00"
	.align 4
LC$0001: 
	.STRING "union \x00"
	.align 4
LC$0002: 
	.STRING "enum \x00"
	.align 4
	.EXPORT type_name_no_tag,CODE
	.EXPORT type_name_no_tag,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
type_name_no_tag: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,8(%r4)
	stw %r5,12(%r4)
	copy %r26,%r5
	ldw 4(%r5),%r6
	comiclr,<> 0,%r6,%r0
	bl L$0062,%r0
	nop
	ldw 0(%r5),%r19
	comiclr,<> 4,%r19,%r0
	bl L$0066,%r0
	nop
	comiclr,>= 4,%r19,%r0
	bl L$0072,%r0
	nop
	comiclr,<> 3,%r19,%r0
	bl L$0064,%r0
	nop
	bl,n L$0070,%r0
L$0072: 
	comiclr,<> 5,%r19,%r0
	bl L$0068,%r0
	nop
	bl,n L$0070,%r0
L$0064: 
	copy %r6,%r26
	ldil L'LC$0000,%r25
	ldo R'LC$0000(%r25),%r25
	ldo 7(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl strncmp,%r2
	nop
	copy %r28,%r19
	comiclr,= 0,%r19,%r0
	bl L$0065,%r0
	nop
	ldo 7(%r6),%r6
L$0065: 
	bl,n L$0063,%r0
L$0066: 
	copy %r6,%r26
	ldil L'LC$0001,%r25
	ldo R'LC$0001(%r25),%r25
	ldo 6(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl strncmp,%r2
	nop
	copy %r28,%r19
	comiclr,= 0,%r19,%r0
	bl L$0067,%r0
	nop
	ldo 6(%r6),%r6
L$0067: 
	bl,n L$0063,%r0
L$0068: 
	copy %r6,%r26
	ldil L'LC$0002,%r25
	ldo R'LC$0002(%r25),%r25
	ldo 5(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl strncmp,%r2
	nop
	copy %r28,%r19
	comiclr,= 0,%r19,%r0
	bl L$0069,%r0
	nop
	ldo 5(%r6),%r6
L$0069: 
	bl,n L$0063,%r0
L$0070: 
	bl,n L$0063,%r0
L$0063: 
L$0062: 
	copy %r6,%r28
	bl,n L$0061,%r0
L$0061: 
	ldw 8(%r4),%r6
	ldw 12(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT current_language,DATA
	.IMPORT strcmp,CODE
	.align 4
	.EXPORT lookup_primitive_typename,CODE
	.EXPORT lookup_primitive_typename,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_primitive_typename: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	addil L'current_language-$global$,%r27
	ldw R'current_language-$global$(%r1),%r19
	ldw 8(%r19),%r20
	stw %r20,8(%r4)
L$0074: 
	ldw 8(%r4),%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0075,%r0
	nop
	ldw 8(%r4),%r19
	ldw 0(%r19),%r20
	ldw 0(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 4(%r19),%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2
	nop
	copy %r28,%r19
	comiclr,= 0,%r19,%r0
	bl L$0077,%r0
	nop
	ldw 8(%r4),%r19
	ldw 0(%r19),%r20
	ldw 0(%r20),%r28
	bl,n L$0073,%r0
L$0077: 
L$0076: 
	ldw 8(%r4),%r19
	ldo 4(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0074,%r0
L$0075: 
	copy %r0,%r28
	bl,n L$0073,%r0
L$0073: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT lookup_symbol,CODE
	.IMPORT error,CODE
	.align 4
LC$0003: 
	.STRING "No type named %s.\x00"
	.align 4
	.EXPORT lookup_typename,CODE
	.EXPORT lookup_typename,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,RTNVAL=GR
lookup_typename: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r12,8(%r4)
	stw %r11,12(%r4)
	stw %r10,16(%r4)
	stw %r9,20(%r4)
	stw %r8,24(%r4)
	stw %r7,28(%r4)
	stw %r6,32(%r4)
	stw %r5,36(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	stw %r0,-52(%r30)
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	ldo 1(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	copy %r28,%r11
	comiclr,<> 0,%r11,%r0
	bl L$0080,%r0
	nop
	ldw 8(%r11),%r19
	comiclr,= 8,%r19,%r0
	bl L$0080,%r0
	nop
	bl,n L$0079,%r0
L$0080: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl lookup_primitive_typename,%r2
	nop
	copy %r28,%r12
	comiclr,<> 0,%r12,%r0
	bl L$0081,%r0
	nop
	copy %r12,%r28
	bl,n L$0078,%r0
	bl,n L$0082,%r0
L$0081: 
	comiclr,= 0,%r12,%r0
	bl L$0083,%r0
	nop
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0083,%r0
	nop
	copy %r0,%r28
	bl,n L$0078,%r0
	bl,n L$0084,%r0
L$0083: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0003,%r26
	ldo R'LC$0003(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0084: 
L$0082: 
L$0079: 
	ldw 12(%r11),%r28
	bl,n L$0078,%r0
L$0078: 
	ldw 8(%r4),%r12
	ldw 12(%r4),%r11
	ldw 16(%r4),%r10
	ldw 20(%r4),%r9
	ldw 24(%r4),%r8
	ldw 28(%r4),%r7
	ldw 32(%r4),%r6
	ldw 36(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT alloca,CODE
	.IMPORT strlen,CODE
	.IMPORT strcpy,CODE
	.align 4
LC$0004: 
	.STRING "unsigned \x00"
	.align 4
	.EXPORT lookup_unsigned_typename,CODE
	.EXPORT lookup_unsigned_typename,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_unsigned_typename: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl strlen,%r2
	nop
	copy %r28,%r19
	ldo 10(%r19),%r20
	ldo 7(%r20),%r21
	copy %r21,%r19
	ldo 63(%r19),%r20
	extru %r20,25,26,%r19
	zdep %r19,25,26,%r20
	ldo -96(%r30),%r19
	add %r30,%r20,%r30
	ldo 7(%r19),%r20
	extru %r20,28,29,%r19
	zdep %r19,28,29,%r20
	stw %r20,8(%r4)
	ldw 8(%r4),%r26
	ldil L'LC$0004,%r25
	ldo R'LC$0004(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcpy,%r2
	nop
	ldw 8(%r4),%r20
	ldo 9(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	copy %r19,%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcpy,%r2
	nop
	ldw 8(%r4),%r26
	copy %r0,%r25
	copy %r0,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl lookup_typename,%r2
	nop
	bl,n L$0085,%r0
L$0085: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
LC$0005: 
	.STRING "signed \x00"
	.align 4
	.EXPORT lookup_signed_typename,CODE
	.EXPORT lookup_signed_typename,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
lookup_signed_typename: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl strlen,%r2
	nop
	copy %r28,%r19
	ldo 8(%r19),%r20
	ldo 7(%r20),%r21
	copy %r21,%r19
	ldo 63(%r19),%r20
	extru %r20,25,26,%r19
	zdep %r19,25,26,%r20
	ldo -96(%r30),%r19
	add %r30,%r20,%r30
	ldo 7(%r19),%r20
	extru %r20,28,29,%r19
	zdep %r19,28,29,%r20
	stw %r20,12(%r4)
	ldw 12(%r4),%r26
	ldil L'LC$0005,%r25
	ldo R'LC$0005(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcpy,%r2
	nop
	ldw 12(%r4),%r20
	ldo 7(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	copy %r19,%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcpy,%r2
	nop
	ldw 12(%r4),%r26
	copy %r0,%r25
	ldo 1(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl lookup_typename,%r2
	nop
	stw %r28,8(%r4)
	ldw 8(%r4),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0087,%r0
	nop
	ldw 8(%r4),%r28
	bl,n L$0086,%r0
L$0087: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	copy %r0,%r25
	copy %r0,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl lookup_typename,%r2
	nop
	bl,n L$0086,%r0
L$0086: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
LC$0006: 
	.STRING "No struct type named %s.\x00"
	.align 4
LC$0007: 
	.STRING "This context has class, union or enum %s, not a struct.\x00"
	.align 4
	.EXPORT lookup_struct,CODE
	.EXPORT lookup_struct,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
lookup_struct: 
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
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	stw %r0,-52(%r30)
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	ldo 2(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	copy %r28,%r9
	comiclr,= 0,%r9,%r0
	bl L$0089,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0006,%r26
	ldo R'LC$0006(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0089: 
	ldw 12(%r9),%r19
	ldw 0(%r19),%r20
	comiclr,<> 3,%r20,%r0
	bl L$0090,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0007,%r26
	ldo R'LC$0007(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0090: 
	ldw 12(%r9),%r28
	bl,n L$0088,%r0
L$0088: 
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
LC$0008: 
	.STRING "No union type named %s.\x00"
	.align 4
LC$0009: 
	.STRING "This context has class, struct or enum %s, not a union.\x00"
	.align 4
	.EXPORT lookup_union,CODE
	.EXPORT lookup_union,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
lookup_union: 
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
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	stw %r0,-52(%r30)
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	ldo 2(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	copy %r28,%r9
	comiclr,= 0,%r9,%r0
	bl L$0092,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0008,%r26
	ldo R'LC$0008(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0092: 
	ldw 12(%r9),%r19
	ldw 0(%r19),%r20
	comiclr,<> 4,%r20,%r0
	bl L$0093,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0009,%r26
	ldo R'LC$0009(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0093: 
	ldw 12(%r9),%r28
	bl,n L$0091,%r0
L$0091: 
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
LC$0010: 
	.STRING "No enum type named %s.\x00"
	.align 4
LC$0011: 
	.STRING "This context has class, struct or union %s, not an enum.\x00"
	.align 4
	.EXPORT lookup_enum,CODE
	.EXPORT lookup_enum,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
lookup_enum: 
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
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	stw %r0,-52(%r30)
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	ldo 2(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	copy %r28,%r9
	comiclr,= 0,%r9,%r0
	bl L$0095,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0010,%r26
	ldo R'LC$0010(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0095: 
	ldw 12(%r9),%r19
	ldw 0(%r19),%r20
	comiclr,<> 5,%r20,%r0
	bl L$0096,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0011,%r26
	ldo R'LC$0011(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0096: 
	ldw 12(%r9),%r28
	bl,n L$0094,%r0
L$0094: 
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
	.IMPORT strcat,CODE
	.align 4
LC$0012: 
	.STRING "<\x00"
	.align 4
LC$0013: 
	.STRING " >\x00"
	.align 4
LC$0014: 
	.STRING "No template type named %s.\x00"
	.align 4
	.EXPORT lookup_template_type,CODE
	.EXPORT lookup_template_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,RTNVAL=GR
lookup_template_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r11,16(%r4)
	stw %r10,20(%r4)
	stw %r9,24(%r4)
	stw %r8,28(%r4)
	stw %r7,32(%r4)
	stw %r6,36(%r4)
	stw %r5,40(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl strlen,%r2
	nop
	copy %r28,%r11
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 4(%r19),%r26
	.CALL ARGW0=GR
	bl strlen,%r2
	nop
	copy %r28,%r19
	add %r11,%r19,%r20
	ldo 4(%r20),%r19
	ldo 7(%r19),%r20
	copy %r20,%r19
	ldo 63(%r19),%r20
	extru %r20,25,26,%r19
	zdep %r19,25,26,%r20
	ldo -96(%r30),%r19
	add %r30,%r20,%r30
	ldo 7(%r19),%r20
	extru %r20,28,29,%r19
	zdep %r19,28,29,%r20
	stw %r20,12(%r4)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 12(%r4),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcpy,%r2
	nop
	ldw 12(%r4),%r26
	ldil L'LC$0012,%r25
	ldo R'LC$0012(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcat,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r4),%r26
	ldw 4(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcat,%r2
	nop
	ldw 12(%r4),%r26
	ldil L'LC$0013,%r25
	ldo R'LC$0013(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcat,%r2
	nop
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	stw %r0,-52(%r30)
	ldw 12(%r4),%r26
	ldw 0(%r19),%r25
	ldo 1(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	stw %r28,8(%r4)
	ldw 8(%r4),%r19
	comiclr,= 0,%r19,%r0
	bl L$0098,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0014,%r26
	ldo R'LC$0014(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0098: 
	ldw 8(%r4),%r19
	ldw 12(%r19),%r20
	ldw 0(%r20),%r19
	comiclr,<> 3,%r19,%r0
	bl L$0099,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0007,%r26
	ldo R'LC$0007(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0099: 
	ldw 8(%r4),%r19
	ldw 12(%r19),%r28
	bl,n L$0097,%r0
L$0097: 
	ldw 16(%r4),%r11
	ldw 20(%r4),%r10
	ldw 24(%r4),%r9
	ldw 28(%r4),%r8
	ldw 32(%r4),%r7
	ldw 36(%r4),%r6
	ldw 40(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT current_target,DATA
	.IMPORT fflush,CODE
	.IMPORT __iob,DATA
	.IMPORT fprintf,CODE
	.align 4
LC$0015: 
	.STRING "Type \x00"
	.IMPORT type_print,CODE
	.align 4
LC$0016: 
	.STRING "\x00"
	.align 4
LC$0017: 
	.STRING " is not a structure or union type.\x00"
	.IMPORT check_stub_type,CODE
	.align 4
LC$0018: 
	.STRING " has no component named \x00"
	.IMPORT fputs_filtered,CODE
	.align 4
LC$0019: 
	.STRING ".\x00"
	.align 4
	.EXPORT lookup_struct_elt_type,CODE
	.EXPORT lookup_struct_elt_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,RTNVAL=GR
lookup_struct_elt_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r11,24(%r4)
	stw %r10,28(%r4)
	stw %r9,32(%r4)
	stw %r8,36(%r4)
	stw %r7,40(%r4)
	stw %r6,44(%r4)
	stw %r5,48(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,<> 1,%r20,%r0
	bl L$0102,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	ldo 16(%r0),%r19
	comclr,<> %r20,%r19,%r0
	bl L$0102,%r0
	nop
	bl,n L$0101,%r0
L$0102: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 16(%r20),%r21
	stw %r21,0(%r19)
L$0101: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,<> 3,%r20,%r0
	bl L$0103,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,<> 4,%r20,%r0
	bl L$0103,%r0
	nop
	addil L'current_target-$global$,%r27
	ldw R'current_target-$global$(%r1),%r19
	ldw 76(%r19),%r11
	copy %r11,%r22
	.CALL	ARGW0=GR
	bl $$dyncall,%r31
	copy %r31,%r2
	addil L'__iob-$global$+16,%r27
	ldo R'__iob-$global$+16(%r1),%r26
	.CALL ARGW0=GR
	bl fflush,%r2
	nop
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r26
	ldil L'LC$0015,%r25
	ldo R'LC$0015(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl fprintf,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	ldil L'LC$0016,%r25
	ldo R'LC$0016(%r25),%r25
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r24
	ldo -1(%r0),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl type_print,%r2
	nop
	ldil L'LC$0017,%r26
	ldo R'LC$0017(%r26),%r26
	.CALL ARGW0=GR
	bl error,%r2
	nop
L$0103: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl check_stub_type,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 34(%r19),%r20
	extrs %r20,31,16,%r19
	ldo -1(%r19),%r20
	stw %r20,8(%r4)
L$0104: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 0(%r20),%r21
	extrs %r21,31,16,%r19
	ldw 8(%r4),%r20
	comclr,>= %r20,%r19,%r0
	bl L$0105,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldw 12(%r19),%r20
	stw %r20,12(%r4)
	ldw 12(%r4),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0107,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 12(%r4),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2
	nop
	copy %r28,%r19
	comiclr,= 0,%r19,%r0
	bl L$0107,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldw 8(%r19),%r28
	bl,n L$0100,%r0
L$0107: 
L$0106: 
	ldw 8(%r4),%r19
	ldo -1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0104,%r0
L$0105: 
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 0(%r20),%r21
	extrs %r21,31,16,%r19
	ldo -1(%r19),%r20
	stw %r20,8(%r4)
L$0108: 
	ldw 8(%r4),%r19
	comiclr,<= 0,%r19,%r0
	bl L$0109,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 8(%r19),%r26
	ldw 0(%r20),%r25
	copy %r0,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl lookup_struct_elt_type,%r2
	nop
	stw %r28,16(%r4)
	ldw 16(%r4),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0111,%r0
	nop
	ldw 16(%r4),%r28
	bl,n L$0100,%r0
L$0111: 
L$0110: 
	ldw 8(%r4),%r19
	ldo -1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0108,%r0
L$0109: 
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0112,%r0
	nop
	copy %r0,%r28
	bl,n L$0100,%r0
L$0112: 
	addil L'current_target-$global$,%r27
	ldw R'current_target-$global$(%r1),%r19
	ldw 76(%r19),%r11
	copy %r11,%r22
	.CALL	ARGW0=GR
	bl $$dyncall,%r31
	copy %r31,%r2
	addil L'__iob-$global$+16,%r27
	ldo R'__iob-$global$+16(%r1),%r26
	.CALL ARGW0=GR
	bl fflush,%r2
	nop
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r26
	ldil L'LC$0015,%r25
	ldo R'LC$0015(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl fprintf,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	ldil L'LC$0016,%r25
	ldo R'LC$0016(%r25),%r25
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r24
	ldo -1(%r0),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl type_print,%r2
	nop
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r26
	ldil L'LC$0018,%r25
	ldo R'LC$0018(%r25),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl fprintf,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	addil L'__iob-$global$+32,%r27
	ldo R'__iob-$global$+32(%r1),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl fputs_filtered,%r2
	nop
	ldil L'LC$0019,%r26
	ldo R'LC$0019(%r26),%r26
	.CALL ARGW0=GR
	bl error,%r2
	nop
	ldo -1(%r0),%r28
	bl,n L$0100,%r0
L$0100: 
	ldw 24(%r4),%r11
	ldw 28(%r4),%r10
	ldw 32(%r4),%r9
	ldw 36(%r4),%r8
	ldw 40(%r4),%r7
	ldw 44(%r4),%r6
	ldw 48(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT fill_in_vptr_fieldno,CODE
	.EXPORT fill_in_vptr_fieldno,ENTRY,PRIV_LEV=3,ARGW0=GR
fill_in_vptr_fieldno: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 44(%r19),%r20
	comiclr,> 0,%r20,%r0
	bl L$0114,%r0
	nop
	ldo 1(%r0),%r19
	stw %r19,8(%r4)
L$0115: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 0(%r20),%r21
	extrs %r21,31,16,%r19
	ldw 8(%r4),%r20
	comclr,< %r20,%r19,%r0
	bl L$0116,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldw 8(%r19),%r26
	.CALL ARGW0=GR
	bl fill_in_vptr_fieldno,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldw 8(%r19),%r20
	ldw 44(%r20),%r19
	comiclr,<= 0,%r19,%r0
	bl L$0118,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 8(%r4),%r21
	zdep %r21,27,28,%r22
	ldw 36(%r20),%r21
	add %r22,%r21,%r20
	ldw 8(%r20),%r21
	ldw 44(%r21),%r20
	stw %r20,44(%r19)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 8(%r4),%r21
	zdep %r21,27,28,%r22
	ldw 36(%r20),%r21
	add %r22,%r21,%r20
	ldw 8(%r20),%r21
	ldw 40(%r21),%r20
	stw %r20,40(%r19)
	bl,n L$0116,%r0
L$0118: 
L$0117: 
	ldw 8(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0115,%r0
L$0116: 
L$0114: 
L$0113: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.EXPORT stub_noname_complaint,DATA
	.align 4
LC$0020: 
	.STRING "stub type has NULL name\x00"
	.data

	.align 4
stub_noname_complaint: 
	.word LC$0020
	.word 0
	.word 0
	.IMPORT complain,CODE
	.IMPORT memcpy,CODE
	.code

	.align 4
	.EXPORT check_stub_type,CODE
	.EXPORT check_stub_type,ENTRY,PRIV_LEV=3,ARGW0=GR
check_stub_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 32(%r19),%r20
	ldo 4(%r0),%r21
	and %r20,%r21,%r19
	extrs %r19,31,16,%r20
	comiclr,<> 0,%r20,%r0
	bl L$0120,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl type_name_no_tag,%r2
	nop
	stw %r28,8(%r4)
	ldw 8(%r4),%r19
	comiclr,= 0,%r19,%r0
	bl L$0121,%r0
	nop
	addil L'stub_noname_complaint-$global$,%r27
	ldo R'stub_noname_complaint-$global$(%r1),%r26
	copy %r0,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl complain,%r2
	nop
	bl,n L$0119,%r0
L$0121: 
	stw %r0,-52(%r30)
	ldw 8(%r4),%r26
	copy %r0,%r25
	ldo 2(%r0),%r24
	copy %r0,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl lookup_symbol,%r2
	nop
	stw %r28,12(%r4)
	ldw 12(%r4),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0122,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 12(%r4),%r20
	ldw 0(%r19),%r26
	ldw 12(%r20),%r25
	ldo 52(%r0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memcpy,%r2
	nop
L$0122: 
L$0120: 
L$0119: 
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT gdb_mangle_name,CODE
	.IMPORT cplus_demangle,CODE
	.align 4
LC$0021: 
	.STRING "Internal: Cannot demangle mangled name `%s'.\x00"
	.IMPORT strchr,CODE
	.IMPORT parse_and_eval_type,CODE
	.IMPORT builtin_type_void,DATA
	.IMPORT free,CODE
	.align 4
	.EXPORT check_stub_method,CODE
	.EXPORT check_stub_method,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR
check_stub_method: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r11,64(%r4)
	stw %r10,68(%r4)
	stw %r9,72(%r4)
	stw %r8,76(%r4)
	stw %r7,80(%r4)
	stw %r6,84(%r4)
	stw %r5,88(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -12(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	ldw 0(%r21),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl gdb_mangle_name,%r2
	nop
	stw %r28,12(%r4)
	ldw 12(%r4),%r26
	ldo 3(%r0),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl cplus_demangle,%r2
	nop
	stw %r28,16(%r4)
	stw %r0,28(%r4)
	ldo 1(%r0),%r19
	stw %r19,32(%r4)
	ldw 16(%r4),%r19
	comiclr,= 0,%r19,%r0
	bl L$0124,%r0
	nop
	ldil L'LC$0021,%r26
	ldo R'LC$0021(%r26),%r26
	ldw 12(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
L$0124: 
	ldw 16(%r4),%r26
	ldo 40(%r0),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strchr,%r2
	nop
	copy %r28,%r19
	ldo 1(%r19),%r20
	stw %r20,20(%r4)
	ldw 20(%r4),%r19
	stw %r19,24(%r4)
L$0125: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	comiclr,<> 0,%r19,%r0
	bl L$0126,%r0
	nop
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 40(%r0),%r20
	comclr,= %r19,%r20,%r0
	bl L$0127,%r0
	nop
	ldw 28(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,28(%r4)
	bl,n L$0128,%r0
L$0127: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 41(%r0),%r20
	comclr,= %r19,%r20,%r0
	bl L$0129,%r0
	nop
	ldw 28(%r4),%r19
	ldo -1(%r19),%r20
	stw %r20,28(%r4)
	bl,n L$0130,%r0
L$0129: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 44(%r0),%r20
	comclr,= %r19,%r20,%r0
	bl L$0131,%r0
	nop
	ldw 28(%r4),%r19
	comiclr,= 0,%r19,%r0
	bl L$0131,%r0
	nop
	ldw 32(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,32(%r4)
L$0131: 
L$0130: 
L$0128: 
	ldw 24(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,24(%r4)
	bl,n L$0125,%r0
L$0126: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0137,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	ldo 120(%r20),%r19
	stw %r19,44(%r4)
	ldw 44(%r4),%r19
	stw %r19,48(%r4)
	ldw 32(%r4),%r20
	ldo 2(%r20),%r19
	zdep %r19,29,30,%r20
	stw %r20,52(%r4)
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 52(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0132,%r0
	nop
	ldw 48(%r4),%r26
	ldw 52(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0133,%r0
L$0132: 
	copy %r0,%r19
L$0133: 
	ldw 48(%r4),%r19
	ldw 48(%r4),%r20
	ldw 12(%r20),%r21
	ldw 52(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 44(%r4),%r19
	stw %r19,56(%r4)
	ldw 56(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,60(%r4)
	ldw 56(%r4),%r19
	ldw 12(%r19),%r20
	ldw 60(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0134,%r0
	nop
	ldw 56(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0134: 
	ldw 56(%r4),%r19
	ldw 56(%r4),%r20
	ldw 56(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 56(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 56(%r4),%r19
	ldw 56(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 56(%r4),%r20
	ldw 56(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0135,%r0
	nop
	ldw 56(%r4),%r19
	ldw 56(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0136,%r0
L$0135: 
	copy %r0,%r19
L$0136: 
	ldw 56(%r4),%r19
	ldw 56(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 60(%r4),%r11
	bl,n L$0138,%r0
L$0137: 
	ldw 32(%r4),%r20
	ldo 2(%r20),%r19
	zdep %r19,29,30,%r20
	copy %r20,%r26
	.CALL ARGW0=GR
	bl xmalloc,%r2
	nop
	copy %r28,%r11
L$0138: 
	stw %r11,36(%r4)
	ldw 20(%r4),%r19
	stw %r19,24(%r4)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl lookup_pointer_type,%r2
	nop
	copy %r28,%r19
	ldw 36(%r4),%r20
	stw %r19,0(%r20)
	ldo 1(%r0),%r19
	stw %r19,32(%r4)
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 41(%r0),%r20
	comclr,<> %r19,%r20,%r0
	bl L$0139,%r0
	nop
	stw %r0,28(%r4)
L$0140: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	comiclr,<> 0,%r19,%r0
	bl L$0141,%r0
	nop
	ldw 28(%r4),%r19
	comiclr,>= 0,%r19,%r0
	bl L$0142,%r0
	nop
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 44(%r0),%r20
	comclr,<> %r19,%r20,%r0
	bl L$0143,%r0
	nop
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 41(%r0),%r20
	comclr,<> %r19,%r20,%r0
	bl L$0143,%r0
	nop
	bl,n L$0142,%r0
L$0143: 
	ldw 24(%r4),%r19
	ldw 20(%r4),%r20
	sub %r19,%r20,%r19
	ldw 20(%r4),%r26
	copy %r19,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl parse_and_eval_type,%r2
	nop
	copy %r28,%r19
	ldw 32(%r4),%r20
	zdep %r20,29,30,%r21
	ldw 36(%r4),%r22
	add %r21,%r22,%r20
	stw %r19,0(%r20)
	ldw 32(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,32(%r4)
	ldw 24(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,20(%r4)
L$0142: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 40(%r0),%r20
	comclr,= %r19,%r20,%r0
	bl L$0144,%r0
	nop
	ldw 28(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,28(%r4)
	bl,n L$0145,%r0
L$0144: 
	ldw 24(%r4),%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 41(%r0),%r20
	comclr,= %r19,%r20,%r0
	bl L$0146,%r0
	nop
	ldw 28(%r4),%r19
	ldo -1(%r19),%r20
	stw %r20,28(%r4)
L$0146: 
L$0145: 
	ldw 24(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,24(%r4)
	bl,n L$0140,%r0
L$0141: 
L$0139: 
	ldo -2(%r0),%r19
	ldw 24(%r4),%r20
	add %r19,%r20,%r19
	ldb 0(%r19),%r20
	extrs %r20,31,8,%r19
	ldo 46(%r0),%r20
	comclr,<> %r19,%r20,%r0
	bl L$0147,%r0
	nop
	ldw 32(%r4),%r19
	zdep %r19,29,30,%r20
	ldw 36(%r4),%r21
	add %r20,%r21,%r19
	addil L'builtin_type_void-$global$,%r27
	ldw R'builtin_type_void-$global$(%r1),%r20
	stw %r20,0(%r19)
	bl,n L$0148,%r0
L$0147: 
	ldw 32(%r4),%r19
	zdep %r19,29,30,%r20
	ldw 36(%r4),%r21
	add %r20,%r21,%r19
	stw %r0,0(%r19)
L$0148: 
	ldw 16(%r4),%r26
	.CALL ARGW0=GR
	bl free,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	zdep %r21,30,31,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 20(%r19),%r21
	add %r20,%r21,%r19
	ldw 8(%r19),%r20
	stw %r20,8(%r4)
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	zdep %r20,29,30,%r19
	add %r19,%r20,%r19
	zdep %r19,29,30,%r19
	ldw 8(%r4),%r20
	add %r19,%r20,%r19
	ldw 12(%r4),%r20
	stw %r20,0(%r19)
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	zdep %r20,29,30,%r19
	add %r19,%r20,%r19
	zdep %r19,29,30,%r19
	ldw 8(%r4),%r20
	add %r19,%r20,%r19
	ldw 4(%r19),%r20
	stw %r20,40(%r4)
	ldw 40(%r4),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	stw %r21,40(%r19)
	ldw 40(%r4),%r19
	ldw 36(%r4),%r20
	stw %r20,48(%r19)
	ldw 40(%r4),%r19
	ldw 40(%r4),%r20
	ldh 32(%r20),%r21
	copy %r21,%r20
	depi 0,29,1,%r20
	sth %r20,32(%r19)
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	zdep %r20,29,30,%r19
	add %r19,%r20,%r19
	zdep %r19,29,30,%r19
	ldw 8(%r4),%r20
	add %r19,%r20,%r19
	ldw 16(%r19),%r20
	copy %r20,%r21
	depi 0,4,1,%r21
	stw %r21,16(%r19)
L$0123: 
	ldw 64(%r4),%r11
	ldw 68(%r4),%r10
	ldw 72(%r4),%r9
	ldw 76(%r4),%r8
	ldw 80(%r4),%r7
	ldw 84(%r4),%r6
	ldw 88(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT allocate_cplus_struct_type,CODE
	.EXPORT allocate_cplus_struct_type,ENTRY,PRIV_LEV=3,ARGW0=GR
allocate_cplus_struct_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r8,32(%r4)
	stw %r7,36(%r4)
	stw %r6,40(%r4)
	stw %r5,44(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	comclr,= %r19,%r20,%r0
	bl L$0150,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r7
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0156,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 12(%r19),%r20
	ldo 120(%r20),%r19
	stw %r19,8(%r4)
	ldw 8(%r4),%r19
	stw %r19,12(%r4)
	ldo 24(%r0),%r19
	stw %r19,16(%r4)
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 16(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0151,%r0
	nop
	ldw 12(%r4),%r26
	ldw 16(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0152,%r0
L$0151: 
	copy %r0,%r19
L$0152: 
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 12(%r20),%r21
	ldw 16(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 8(%r4),%r19
	stw %r19,20(%r4)
	ldw 20(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,24(%r4)
	ldw 20(%r4),%r19
	ldw 12(%r19),%r20
	ldw 24(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0153,%r0
	nop
	ldw 20(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0153: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 20(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0154,%r0
	nop
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0155,%r0
L$0154: 
	copy %r0,%r19
L$0155: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 24(%r4),%r8
	bl,n L$0157,%r0
L$0156: 
	ldo 24(%r0),%r26
	.CALL ARGW0=GR
	bl xmalloc,%r2
	nop
	copy %r28,%r8
L$0157: 
	stw %r8,48(%r7)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	copy %r20,%r21
	ldws,ma 4(%r22),%r19
	ldws,ma 4(%r22),%r20
	stws,ma %r19,4(%r21)
	ldws,ma 4(%r22),%r19
	stws,ma %r20,4(%r21)
	ldws,ma 4(%r22),%r20
	stws,ma %r19,4(%r21)
	ldws,ma 4(%r22),%r19
	stws,ma %r20,4(%r21)
	ldws,ma 4(%r22),%r20
	stws,ma %r19,4(%r21)
	stw %r20,0(%r21)
L$0150: 
L$0149: 
	ldw 32(%r4),%r8
	ldw 36(%r4),%r7
	ldw 40(%r4),%r6
	ldw 44(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT obsavestring,CODE
	.align 4
	.EXPORT init_type,CODE
	.EXPORT init_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR
init_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r14,8(%r4)
	stw %r13,12(%r4)
	stw %r12,16(%r4)
	stw %r11,20(%r4)
	stw %r10,24(%r4)
	stw %r9,28(%r4)
	stw %r8,32(%r4)
	stw %r7,36(%r4)
	stw %r6,40(%r4)
	stw %r5,44(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -12(%r0),%r9
	ldo -32(%r4),%r19
	add %r19,%r9,%r10
	stw %r24,0(%r10)
	ldo -16(%r0),%r11
	ldo -32(%r4),%r19
	add %r19,%r11,%r12
	stw %r23,0(%r12)
	ldo -20(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl alloc_type,%r2
	nop
	copy %r28,%r13
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,0(%r13)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,8(%r13)
	ldo -12(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldh 32(%r13),%r20
	ldh 2(%r19),%r19
	or %r20,%r19,%r20
	sth %r20,32(%r13)
	ldo -16(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0159,%r0
	nop
	ldo -20(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0159,%r0
	nop
	ldo -16(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r14
	ldo -16(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r26
	.CALL ARGW0=GR
	bl strlen,%r2
	nop
	copy %r28,%r19
	ldo -20(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 120(%r21),%r20
	ldw 0(%r14),%r26
	copy %r19,%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl obsavestring,%r2
	nop
	copy %r28,%r19
	stw %r19,4(%r13)
	bl,n L$0160,%r0
L$0159: 
	ldo -16(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,4(%r13)
L$0160: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 3,%r20,%r0
	bl L$0162,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 4,%r20,%r0
	bl L$0162,%r0
	nop
	bl,n L$0161,%r0
L$0162: 
	stw %r19,48(%r13)
L$0161: 
	copy %r13,%r28
	bl,n L$0158,%r0
L$0158: 
	ldw 8(%r4),%r14
	ldw 12(%r4),%r13
	ldw 16(%r4),%r12
	ldw 20(%r4),%r11
	ldw 24(%r4),%r10
	ldw 28(%r4),%r9
	ldw 32(%r4),%r8
	ldw 36(%r4),%r7
	ldw 40(%r4),%r6
	ldw 44(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
LC$0022: 
	.STRING "internal error - invalid fundamental type id %d\x00"
	.align 4
LC$0023: 
	.STRING "internal error: unhandled type id %d\x00"
	.align 4
LC$0024: 
	.STRING "void\x00"
	.align 4
LC$0025: 
	.STRING "boolean\x00"
	.align 4
LC$0026: 
	.STRING "string\x00"
	.align 4
LC$0027: 
	.STRING "char\x00"
	.align 4
LC$0028: 
	.STRING "signed char\x00"
	.align 4
LC$0029: 
	.STRING "unsigned char\x00"
	.align 4
LC$0030: 
	.STRING "short\x00"
	.align 4
LC$0031: 
	.STRING "unsigned short\x00"
	.align 4
LC$0032: 
	.STRING "int\x00"
	.align 4
LC$0033: 
	.STRING "unsigned int\x00"
	.align 4
LC$0034: 
	.STRING "fixed decimal\x00"
	.align 4
LC$0035: 
	.STRING "long\x00"
	.align 4
LC$0036: 
	.STRING "unsigned long\x00"
	.align 4
LC$0037: 
	.STRING "long long\x00"
	.align 4
LC$0038: 
	.STRING "signed long long\x00"
	.align 4
LC$0039: 
	.STRING "unsigned long long\x00"
	.align 4
LC$0040: 
	.STRING "float\x00"
	.align 4
LC$0041: 
	.STRING "double\x00"
	.align 4
LC$0042: 
	.STRING "floating decimal\x00"
	.align 4
LC$0043: 
	.STRING "long double\x00"
	.align 4
LC$0044: 
	.STRING "complex\x00"
	.align 4
LC$0045: 
	.STRING "double complex\x00"
	.align 4
LC$0046: 
	.STRING "long double complex\x00"
	.align 4
	.EXPORT lookup_fundamental_type,CODE
	.EXPORT lookup_fundamental_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
lookup_fundamental_type: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r12,32(%r4)
	stw %r11,36(%r4)
	stw %r10,40(%r4)
	stw %r9,44(%r4)
	stw %r8,48(%r4)
	stw %r7,52(%r4)
	stw %r6,56(%r4)
	stw %r5,60(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	copy %r0,%r9
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<= 0,%r20,%r0
	bl L$0165,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 25(%r0),%r19
	comclr,<= %r20,%r19,%r0
	bl L$0165,%r0
	nop
	bl,n L$0164,%r0
L$0165: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0022,%r26
	ldo R'LC$0022(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
	bl,n L$0166,%r0
L$0164: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 196(%r19),%r20
	comiclr,= 0,%r20,%r0
	bl L$0167,%r0
	nop
	ldo 104(%r0),%r11
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r12
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo 120(%r19),%r20
	stw %r20,8(%r4)
	ldw 8(%r4),%r19
	stw %r19,12(%r4)
	stw %r11,16(%r4)
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 16(%r19),%r19
	ldw 12(%r20),%r20
	sub %r19,%r20,%r19
	ldw 16(%r4),%r20
	comclr,< %r19,%r20,%r0
	bl L$0168,%r0
	nop
	ldw 12(%r4),%r26
	ldw 16(%r4),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl _obstack_newchunk,%r2
	nop
	copy %r0,%r19
	bl,n L$0169,%r0
L$0168: 
	copy %r0,%r19
L$0169: 
	ldw 12(%r4),%r19
	ldw 12(%r4),%r20
	ldw 12(%r20),%r21
	ldw 16(%r4),%r22
	add %r21,%r22,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 8(%r4),%r19
	stw %r19,20(%r4)
	ldw 20(%r4),%r19
	ldw 8(%r19),%r20
	stw %r20,24(%r4)
	ldw 20(%r4),%r19
	ldw 12(%r19),%r20
	ldw 24(%r4),%r19
	comclr,= %r20,%r19,%r0
	bl L$0170,%r0
	nop
	ldw 20(%r4),%r19
	ldw 40(%r19),%r20
	copy %r20,%r21
	depi -1,1,1,%r21
	stw %r21,40(%r19)
L$0170: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 12(%r20),%r20
	ldw 24(%r21),%r21
	add %r20,%r21,%r20
	ldw 20(%r4),%r21
	ldw 24(%r21),%r22
	uaddcm %r0,%r22,%r21
	and %r20,%r21,%r20
	copy %r20,%r21
	stw %r21,12(%r19)
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r19),%r19
	ldw 4(%r20),%r20
	sub %r19,%r20,%r19
	ldw 20(%r4),%r20
	ldw 20(%r4),%r21
	ldw 16(%r20),%r20
	ldw 4(%r21),%r21
	sub %r20,%r21,%r20
	comclr,> %r19,%r20,%r0
	bl L$0171,%r0
	nop
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 16(%r20),%r21
	stw %r21,12(%r19)
	copy %r21,%r19
	bl,n L$0172,%r0
L$0171: 
	copy %r0,%r19
L$0172: 
	ldw 20(%r4),%r19
	ldw 20(%r4),%r20
	ldw 12(%r20),%r21
	stw %r21,8(%r19)
	ldw 24(%r4),%r19
	stw %r19,196(%r12)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 196(%r19),%r26
	copy %r0,%r25
	copy %r11,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2
	nop
L$0167: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	zdep %r21,29,30,%r20
	ldw 196(%r19),%r19
	add %r20,%r19,%r10
	ldw 0(%r10),%r9
	comiclr,= 0,%r9,%r0
	bl L$0173,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	addi,uv -26,%r20,%r0
	blr,n %r20,%r0
	b,n L$0175
L$0202: 
	b L$0176
	nop
	b L$0177
	nop
	b L$0179
	nop
	b L$0180
	nop
	b L$0181
	nop
	b L$0182
	nop
	b L$0183
	nop
	b L$0184
	nop
	b L$0185
	nop
	b L$0186
	nop
	b L$0187
	nop
	b L$0189
	nop
	b L$0190
	nop
	b L$0191
	nop
	b L$0192
	nop
	b L$0193
	nop
	b L$0194
	nop
	b L$0195
	nop
	b L$0196
	nop
	b L$0198
	nop
	b L$0199
	nop
	b L$0200
	nop
	b L$0201
	nop
	b L$0178
	nop
	b L$0188
	nop
	b L$0197
	nop
L$0175: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldil L'LC$0023,%r26
	ldo R'LC$0023(%r26),%r26
	ldw 0(%r19),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl error,%r2
	nop
	bl,n L$0174,%r0
L$0176: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 9(%r0),%r26
	ldo 1(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0024,%r23
	ldo R'LC$0024(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0177: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0025,%r23
	ldo R'LC$0025(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0178: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 12(%r0),%r26
	ldo 1(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0026,%r23
	ldo R'LC$0026(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0179: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 1(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0027,%r23
	ldo R'LC$0027(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0180: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 1(%r0),%r25
	ldo 2(%r0),%r24
	ldil L'LC$0028,%r23
	ldo R'LC$0028(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0181: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 1(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0029,%r23
	ldo R'LC$0029(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0182: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 2(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0030,%r23
	ldo R'LC$0030(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0183: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 2(%r0),%r25
	ldo 2(%r0),%r24
	ldil L'LC$0030,%r23
	ldo R'LC$0030(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0184: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 2(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0031,%r23
	ldo R'LC$0031(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0185: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0032,%r23
	ldo R'LC$0032(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0186: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	ldo 2(%r0),%r24
	ldil L'LC$0032,%r23
	ldo R'LC$0032(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0187: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0033,%r23
	ldo R'LC$0033(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0188: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0034,%r23
	ldo R'LC$0034(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0189: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0035,%r23
	ldo R'LC$0035(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0190: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	ldo 2(%r0),%r24
	ldil L'LC$0035,%r23
	ldo R'LC$0035(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0191: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 4(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0036,%r23
	ldo R'LC$0036(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0192: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 8(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0037,%r23
	ldo R'LC$0037(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0193: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 8(%r0),%r25
	ldo 2(%r0),%r24
	ldil L'LC$0038,%r23
	ldo R'LC$0038(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0194: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 7(%r0),%r26
	ldo 8(%r0),%r25
	ldo 1(%r0),%r24
	ldil L'LC$0039,%r23
	ldo R'LC$0039(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0195: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 4(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0040,%r23
	ldo R'LC$0040(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0196: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 8(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0041,%r23
	ldo R'LC$0041(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0197: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 8(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0042,%r23
	ldo R'LC$0042(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0198: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 16(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0043,%r23
	ldo R'LC$0043(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0199: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 8(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0044,%r23
	ldo R'LC$0044(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0200: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 16(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0045,%r23
	ldo R'LC$0045(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0201: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	stw %r20,-52(%r30)
	ldo 8(%r0),%r26
	ldo 16(%r0),%r25
	copy %r0,%r24
	ldil L'LC$0046,%r23
	ldo R'LC$0046(%r23),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl init_type,%r2
	nop
	copy %r28,%r9
	bl,n L$0174,%r0
L$0174: 
	stw %r9,0(%r10)
L$0173: 
L$0166: 
	copy %r9,%r28
	bl,n L$0163,%r0
L$0163: 
	ldw 32(%r4),%r12
	ldw 36(%r4),%r11
	ldw 40(%r4),%r10
	ldw 44(%r4),%r9
	ldw 48(%r4),%r8
	ldw 52(%r4),%r7
	ldw 56(%r4),%r6
	ldw 60(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT puts_filtered,CODE
	.align 4
LC$0047: 
	.STRING " \x00"
	.IMPORT printf_filtered,CODE
	.align 4
LC$0048: 
	.STRING "1\x00"
	.align 4
LC$0049: 
	.STRING "0\x00"
	.align 4
print_bit_vector: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r8,16(%r4)
	stw %r7,20(%r4)
	stw %r6,24(%r4)
	stw %r5,28(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	stw %r0,8(%r4)
L$0204: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 8(%r4),%r20
	ldw 0(%r19),%r19
	comclr,< %r20,%r19,%r0
	bl L$0205,%r0
	nop
	ldw 8(%r4),%r19
	ldw 8(%r4),%r20
	comiclr,> 0,%r19,%r0
	bl L$0208,%r0
	nop
	ldo 7(%r19),%r19
L$0208: 
	extrs %r19,28,29,%r19
	zdep %r19,28,29,%r21
	sub %r20,%r21,%r19
	comiclr,= 0,%r19,%r0
	bl L$0207,%r0
	nop
	ldil L'LC$0047,%r26
	ldo R'LC$0047(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0207: 
	ldw 8(%r4),%r20
	extrs %r20,28,29,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	add %r19,%r21,%r20
	ldb 0(%r20),%r19
	ldw 8(%r4),%r20
	extru %r20,31,3,%r21
	subi,>>= 31,%r21,%r20
	copy %r0,%r20
	mtsar %r20
	vextrs %r19,32,%r19
	extru %r19,31,1,%r20
	comiclr,<> 0,%r20,%r0
	bl L$0209,%r0
	nop
	ldil L'LC$0048,%r26
	ldo R'LC$0048(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0210,%r0
L$0209: 
	ldil L'LC$0049,%r26
	ldo R'LC$0049(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
L$0210: 
L$0206: 
	ldw 8(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0204,%r0
L$0205: 
L$0203: 
	ldw 16(%r4),%r8
	ldw 20(%r4),%r7
	ldw 24(%r4),%r6
	ldw 28(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT recursive_dump_type,CODE
	.align 4
print_arg_types: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r8,8(%r4)
	stw %r7,12(%r4)
	stw %r6,16(%r4)
	stw %r5,20(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0212,%r0
	nop
L$0213: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0214,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 2(%r21),%r20
	ldw 0(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl recursive_dump_type,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 4(%r20),%r21
	stw %r21,0(%r19)
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	comiclr,= 9,%r20,%r0
	bl L$0215,%r0
	nop
	bl,n L$0214,%r0
L$0215: 
	bl,n L$0213,%r0
L$0214: 
L$0212: 
L$0211: 
	ldw 8(%r4),%r8
	ldw 12(%r4),%r7
	ldw 16(%r4),%r6
	ldw 20(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.IMPORT printfi_filtered,CODE
	.align 4
LC$0050: 
	.STRING "fn_fieldlists 0x%x\x0a\x00"
	.align 4
LC$0051: 
	.STRING "[%d] name '%s' (0x%x) length %d\x0a\x00"
	.align 4
LC$0052: 
	.STRING "[%d] physname '%s' (0x%x)\x0a\x00"
	.align 4
LC$0053: 
	.STRING "type 0x%x\x0a\x00"
	.align 4
LC$0054: 
	.STRING "args 0x%x\x0a\x00"
	.align 4
LC$0055: 
	.STRING "fcontext 0x%x\x0a\x00"
	.align 4
LC$0056: 
	.STRING "is_const %d\x0a\x00"
	.align 4
LC$0057: 
	.STRING "is_volatile %d\x0a\x00"
	.align 4
LC$0058: 
	.STRING "is_private %d\x0a\x00"
	.align 4
LC$0059: 
	.STRING "is_protected %d\x0a\x00"
	.align 4
LC$0060: 
	.STRING "is_stub %d\x0a\x00"
	.align 4
LC$0061: 
	.STRING "voffset %u\x0a\x00"
	.align 4
dump_fn_fieldlists: 
	.PROC
	.CALLINFO FRAME=192,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,192(%r30)
	stw %r8,24(%r4)
	stw %r7,28(%r4)
	stw %r6,32(%r4)
	stw %r5,36(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldw 48(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0050,%r25
	ldo R'LC$0050(%r25),%r25
	ldw 20(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	stw %r0,8(%r4)
L$0217: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 2(%r20),%r21
	extrs %r21,31,16,%r19
	ldw 8(%r4),%r20
	comclr,< %r20,%r19,%r0
	bl L$0218,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldw 8(%r4),%r21
	zdep %r21,30,31,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 20(%r19),%r21
	add %r20,%r21,%r19
	ldw 8(%r19),%r20
	stw %r20,16(%r4)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 2(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldw 48(%r21),%r20
	ldw 8(%r4),%r22
	zdep %r22,30,31,%r21
	add %r21,%r22,%r21
	zdep %r21,29,30,%r21
	ldw 20(%r20),%r22
	add %r21,%r22,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 48(%r22),%r21
	ldw 8(%r4),%r23
	zdep %r23,30,31,%r22
	add %r22,%r23,%r22
	zdep %r22,29,30,%r22
	ldw 20(%r21),%r23
	add %r22,%r23,%r21
	ldw 0(%r21),%r22
	stw %r22,-52(%r30)
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 48(%r22),%r21
	ldw 8(%r4),%r23
	zdep %r23,30,31,%r22
	add %r22,%r23,%r22
	zdep %r22,29,30,%r22
	ldw 20(%r21),%r23
	add %r22,%r23,%r21
	ldw 4(%r21),%r22
	stw %r22,-56(%r30)
	copy %r19,%r26
	ldil L'LC$0051,%r25
	ldo R'LC$0051(%r25),%r25
	ldw 8(%r4),%r24
	ldw 0(%r20),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	stw %r0,12(%r4)
L$0220: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldw 8(%r4),%r21
	zdep %r21,30,31,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 20(%r19),%r21
	add %r20,%r21,%r19
	ldw 12(%r4),%r20
	ldw 4(%r19),%r19
	comclr,< %r20,%r19,%r0
	bl L$0221,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 4(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 12(%r4),%r22
	zdep %r22,29,30,%r21
	add %r21,%r22,%r21
	zdep %r21,29,30,%r21
	ldw 16(%r4),%r22
	add %r21,%r22,%r21
	ldw 0(%r21),%r22
	stw %r22,-52(%r30)
	copy %r19,%r26
	ldil L'LC$0052,%r25
	ldo R'LC$0052(%r25),%r25
	ldw 12(%r4),%r24
	ldw 0(%r20),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	copy %r19,%r26
	ldil L'LC$0053,%r25
	ldo R'LC$0053(%r25),%r25
	ldw 4(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldw 12(%r4),%r20
	zdep %r20,29,30,%r19
	add %r19,%r20,%r19
	zdep %r19,29,30,%r19
	ldw 16(%r4),%r20
	add %r19,%r20,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 10(%r21),%r20
	ldw 4(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl recursive_dump_type,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r22
	add %r20,%r22,%r21
	ldw 4(%r21),%r20
	copy %r19,%r26
	ldil L'LC$0054,%r25
	ldo R'LC$0054(%r25),%r25
	ldw 48(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldw 12(%r4),%r20
	zdep %r20,29,30,%r19
	add %r19,%r20,%r19
	zdep %r19,29,30,%r19
	ldw 16(%r4),%r21
	add %r19,%r21,%r20
	ldw 4(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 48(%r19),%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_arg_types,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	copy %r19,%r26
	ldil L'LC$0055,%r25
	ldo R'LC$0055(%r25),%r25
	ldw 12(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,0+1-1,1,%r20
	copy %r19,%r26
	ldil L'LC$0056,%r25
	ldo R'LC$0056(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,1+1-1,1,%r20
	copy %r19,%r26
	ldil L'LC$0057,%r25
	ldo R'LC$0057(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,2+1-1,1,%r20
	copy %r19,%r26
	ldil L'LC$0058,%r25
	ldo R'LC$0058(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,3+1-1,1,%r20
	copy %r19,%r26
	ldil L'LC$0059,%r25
	ldo R'LC$0059(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,4+1-1,1,%r20
	copy %r19,%r26
	ldil L'LC$0060,%r25
	ldo R'LC$0060(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 8(%r20),%r19
	ldw 12(%r4),%r21
	zdep %r21,29,30,%r20
	add %r20,%r21,%r20
	zdep %r20,29,30,%r20
	ldw 16(%r4),%r21
	add %r20,%r21,%r20
	ldw 16(%r20),%r21
	extru %r21,8+24-1,24,%r22
	ldo -2(%r22),%r20
	copy %r19,%r26
	ldil L'LC$0061,%r25
	ldo R'LC$0061(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
L$0222: 
	ldw 12(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,12(%r4)
	bl,n L$0220,%r0
L$0221: 
L$0219: 
	ldw 8(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0217,%r0
L$0218: 
L$0216: 
	ldw 24(%r4),%r8
	ldw 28(%r4),%r7
	ldw 32(%r4),%r6
	ldw 36(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
LC$0062: 
	.STRING "n_baseclasses %d\x0a\x00"
	.align 4
LC$0063: 
	.STRING "nfn_fields %d\x0a\x00"
	.align 4
LC$0064: 
	.STRING "nfn_fields_total %d\x0a\x00"
	.align 4
LC$0065: 
	.STRING "virtual_field_bits (%d bits at *0x%x)\x00"
	.align 4
LC$0066: 
	.STRING "\x0a\x00"
	.align 4
LC$0067: 
	.STRING "private_field_bits (%d bits at *0x%x)\x00"
	.align 4
LC$0068: 
	.STRING "protected_field_bits (%d bits at *0x%x)\x00"
	.align 4
print_cplus_stuff: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r8,16(%r4)
	stw %r7,20(%r4)
	stw %r6,24(%r4)
	stw %r5,28(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 48(%r20),%r21
	ldh 0(%r21),%r22
	extrs %r22,31,16,%r20
	ldw 0(%r19),%r26
	ldil L'LC$0062,%r25
	ldo R'LC$0062(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 48(%r20),%r21
	ldh 2(%r21),%r22
	extrs %r22,31,16,%r20
	ldw 0(%r19),%r26
	ldil L'LC$0063,%r25
	ldo R'LC$0063(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldw 48(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0064,%r25
	ldo R'LC$0064(%r25),%r25
	ldw 4(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 0(%r20),%r21
	extrs %r21,31,16,%r19
	comiclr,< 0,%r19,%r0
	bl L$0224,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 48(%r20),%r21
	ldh 0(%r21),%r22
	extrs %r22,31,16,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 48(%r22),%r21
	ldw 0(%r19),%r26
	ldil L'LC$0065,%r25
	ldo R'LC$0065(%r25),%r25
	copy %r20,%r24
	ldw 8(%r21),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 48(%r20),%r21
	ldh 0(%r21),%r22
	extrs %r22,31,16,%r20
	ldw 8(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_bit_vector,%r2
	nop
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0224: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 34(%r19),%r20
	extrs %r20,31,16,%r19
	comiclr,< 0,%r19,%r0
	bl L$0225,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldw 12(%r20),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0226,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 34(%r20),%r21
	extrs %r21,31,16,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 48(%r22),%r21
	ldw 0(%r19),%r26
	ldil L'LC$0067,%r25
	ldo R'LC$0067(%r25),%r25
	copy %r20,%r24
	ldw 12(%r21),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 34(%r20),%r21
	extrs %r21,31,16,%r20
	ldw 12(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_bit_vector,%r2
	nop
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0226: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldw 16(%r20),%r19
	comiclr,<> 0,%r19,%r0
	bl L$0227,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 34(%r20),%r21
	extrs %r21,31,16,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 48(%r22),%r21
	ldw 0(%r19),%r26
	ldil L'LC$0068,%r25
	ldo R'LC$0068(%r25),%r25
	copy %r20,%r24
	ldw 16(%r21),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 48(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 34(%r20),%r21
	extrs %r21,31,16,%r20
	ldw 16(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_bit_vector,%r2
	nop
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0227: 
L$0225: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	ldh 2(%r20),%r21
	extrs %r21,31,16,%r19
	comiclr,< 0,%r19,%r0
	bl L$0228,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl dump_fn_fieldlists,%r2
	nop
L$0228: 
L$0223: 
	ldw 16(%r4),%r8
	ldw 20(%r4),%r7
	ldw 24(%r4),%r6
	ldw 28(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
	.align 4
LC$0069: 
	.STRING "type node 0x%x\x0a\x00"
	.align 4
LC$0070: 
	.STRING "name '%s' (0x%x)\x0a\x00"
	.align 4
LC$0071: 
	.STRING "<NULL>\x00"
	.align 4
LC$0072: 
	.STRING "code 0x%x \x00"
	.align 4
LC$0073: 
	.STRING "(TYPE_CODE_UNDEF)\x00"
	.align 4
LC$0074: 
	.STRING "(TYPE_CODE_PTR)\x00"
	.align 4
LC$0075: 
	.STRING "(TYPE_CODE_ARRAY)\x00"
	.align 4
LC$0076: 
	.STRING "(TYPE_CODE_STRUCT)\x00"
	.align 4
LC$0077: 
	.STRING "(TYPE_CODE_UNION)\x00"
	.align 4
LC$0078: 
	.STRING "(TYPE_CODE_ENUM)\x00"
	.align 4
LC$0079: 
	.STRING "(TYPE_CODE_FUNC)\x00"
	.align 4
LC$0080: 
	.STRING "(TYPE_CODE_INT)\x00"
	.align 4
LC$0081: 
	.STRING "(TYPE_CODE_FLT)\x00"
	.align 4
LC$0082: 
	.STRING "(TYPE_CODE_VOID)\x00"
	.align 4
LC$0083: 
	.STRING "(TYPE_CODE_SET)\x00"
	.align 4
LC$0084: 
	.STRING "(TYPE_CODE_RANGE)\x00"
	.align 4
LC$0085: 
	.STRING "(TYPE_CODE_PASCAL_ARRAY)\x00"
	.align 4
LC$0086: 
	.STRING "(TYPE_CODE_ERROR)\x00"
	.align 4
LC$0087: 
	.STRING "(TYPE_CODE_MEMBER)\x00"
	.align 4
LC$0088: 
	.STRING "(TYPE_CODE_METHOD)\x00"
	.align 4
LC$0089: 
	.STRING "(TYPE_CODE_REF)\x00"
	.align 4
LC$0090: 
	.STRING "(TYPE_CODE_CHAR)\x00"
	.align 4
LC$0091: 
	.STRING "(TYPE_CODE_BOOL)\x00"
	.align 4
LC$0092: 
	.STRING "(UNKNOWN TYPE CODE)\x00"
	.align 4
LC$0093: 
	.STRING "length %d\x0a\x00"
	.align 4
LC$0094: 
	.STRING "objfile 0x%x\x0a\x00"
	.align 4
LC$0095: 
	.STRING "target_type 0x%x\x0a\x00"
	.align 4
LC$0096: 
	.STRING "pointer_type 0x%x\x0a\x00"
	.align 4
LC$0097: 
	.STRING "reference_type 0x%x\x0a\x00"
	.align 4
LC$0098: 
	.STRING "function_type 0x%x\x0a\x00"
	.align 4
LC$0099: 
	.STRING "flags 0x%x\x00"
	.align 4
LC$0100: 
	.STRING " TYPE_FLAG_UNSIGNED\x00"
	.align 4
LC$0101: 
	.STRING " TYPE_FLAG_SIGNED\x00"
	.align 4
LC$0102: 
	.STRING " TYPE_FLAG_STUB\x00"
	.align 4
LC$0103: 
	.STRING "nfields %d 0x%x\x0a\x00"
	.align 4
LC$0104: 
	.STRING "[%d] bitpos %d bitsize %d type 0x%x name '%s' (0x%x)\x0a\x00"
	.align 4
LC$0105: 
	.STRING "vptr_basetype 0x%x\x0a\x00"
	.align 4
LC$0106: 
	.STRING "vptr_fieldno %d\x0a\x00"
	.align 4
LC$0107: 
	.STRING "arg_types 0x%x\x0a\x00"
	.align 4
LC$0108: 
	.STRING "cplus_stuff 0x%x\x0a\x00"
	.align 4
LC$0109: 
	.STRING "type_specific 0x%x\x00"
	.align 4
LC$0110: 
	.STRING " (unknown data form)\x00"
	.align 4
	.EXPORT recursive_dump_type,CODE
	.EXPORT recursive_dump_type,ENTRY,PRIV_LEV=3,ARGW0=GR,ARGW1=GR
recursive_dump_type: 
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,128(%r30)
	stw %r8,16(%r4)
	stw %r7,20(%r4)
	stw %r6,24(%r4)
	stw %r5,28(%r4)
	ldo -4(%r0),%r5
	ldo -32(%r4),%r19
	add %r19,%r5,%r6
	stw %r26,0(%r6)
	ldo -8(%r0),%r7
	ldo -32(%r4),%r19
	add %r19,%r7,%r8
	stw %r25,0(%r8)
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r19),%r26
	ldil L'LC$0069,%r25
	ldo R'LC$0069(%r25),%r25
	ldw 0(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(%r21),%r22
	ldw 4(%r22),%r21
	ldo -4(%r0),%r22
	ldo -32(%r4),%r24
	add %r24,%r22,%r23
	ldw 0(%r23),%r22
	ldw 4(%r22),%r23
	comiclr,= 0,%r23,%r0
	bl L$0230,%r0
	nop
	ldil L'LC$0071,%r21
	ldo R'LC$0071(%r21),%r21
L$0230: 
	ldw 0(%r19),%r26
	ldil L'LC$0070,%r25
	ldo R'LC$0070(%r25),%r25
	ldw 4(%r20),%r24
	copy %r21,%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0072,%r25
	ldo R'LC$0072(%r25),%r25
	ldw 0(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 0(%r19),%r20
	addi,uv -19,%r20,%r0
	blr,n %r20,%r0
	b,n L$0251
L$0252: 
	b L$0232
	nop
	b L$0233
	nop
	b L$0234
	nop
	b L$0235
	nop
	b L$0236
	nop
	b L$0237
	nop
	b L$0238
	nop
	b L$0239
	nop
	b L$0240
	nop
	b L$0241
	nop
	b L$0242
	nop
	b L$0243
	nop
	b L$0244
	nop
	b L$0245
	nop
	b L$0246
	nop
	b L$0247
	nop
	b L$0248
	nop
	b L$0249
	nop
	b L$0250
	nop
L$0232: 
	ldil L'LC$0073,%r26
	ldo R'LC$0073(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0233: 
	ldil L'LC$0074,%r26
	ldo R'LC$0074(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0234: 
	ldil L'LC$0075,%r26
	ldo R'LC$0075(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0235: 
	ldil L'LC$0076,%r26
	ldo R'LC$0076(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0236: 
	ldil L'LC$0077,%r26
	ldo R'LC$0077(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0237: 
	ldil L'LC$0078,%r26
	ldo R'LC$0078(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0238: 
	ldil L'LC$0079,%r26
	ldo R'LC$0079(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0239: 
	ldil L'LC$0080,%r26
	ldo R'LC$0080(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0240: 
	ldil L'LC$0081,%r26
	ldo R'LC$0081(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0241: 
	ldil L'LC$0082,%r26
	ldo R'LC$0082(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0242: 
	ldil L'LC$0083,%r26
	ldo R'LC$0083(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0243: 
	ldil L'LC$0084,%r26
	ldo R'LC$0084(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0244: 
	ldil L'LC$0085,%r26
	ldo R'LC$0085(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0245: 
	ldil L'LC$0086,%r26
	ldo R'LC$0086(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0246: 
	ldil L'LC$0087,%r26
	ldo R'LC$0087(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0247: 
	ldil L'LC$0088,%r26
	ldo R'LC$0088(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0248: 
	ldil L'LC$0089,%r26
	ldo R'LC$0089(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0249: 
	ldil L'LC$0090,%r26
	ldo R'LC$0090(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0250: 
	ldil L'LC$0091,%r26
	ldo R'LC$0091(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0251: 
	ldil L'LC$0092,%r26
	ldo R'LC$0092(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0231,%r0
L$0231: 
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0093,%r25
	ldo R'LC$0093(%r25),%r25
	ldw 8(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0094,%r25
	ldo R'LC$0094(%r25),%r25
	ldw 12(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0095,%r25
	ldo R'LC$0095(%r25),%r25
	ldw 16(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 16(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0253,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 2(%r21),%r20
	ldw 16(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl recursive_dump_type,%r2
	nop
L$0253: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0096,%r25
	ldo R'LC$0096(%r25),%r25
	ldw 20(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0097,%r25
	ldo R'LC$0097(%r25),%r25
	ldw 24(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0098,%r25
	ldo R'LC$0098(%r25),%r25
	ldw 28(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 32(%r20),%r21
	extrs %r21,31,16,%r20
	ldw 0(%r19),%r26
	ldil L'LC$0099,%r25
	ldo R'LC$0099(%r25),%r25
	copy %r20,%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 32(%r19),%r20
	extru %r20,31,1,%r19
	extrs %r19,31,16,%r20
	comiclr,<> 0,%r20,%r0
	bl L$0254,%r0
	nop
	ldil L'LC$0100,%r26
	ldo R'LC$0100(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0254: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 32(%r19),%r20
	ldo 2(%r0),%r21
	and %r20,%r21,%r19
	extrs %r19,31,16,%r20
	comiclr,<> 0,%r20,%r0
	bl L$0255,%r0
	nop
	ldil L'LC$0101,%r26
	ldo R'LC$0101(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0255: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 32(%r19),%r20
	ldo 4(%r0),%r21
	and %r20,%r21,%r19
	extrs %r19,31,16,%r20
	comiclr,<> 0,%r20,%r0
	bl L$0256,%r0
	nop
	ldil L'LC$0102,%r26
	ldo R'LC$0102(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
L$0256: 
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl puts_filtered,%r2
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldh 34(%r20),%r21
	extrs %r21,31,16,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 0(%r19),%r26
	ldil L'LC$0103,%r25
	ldo R'LC$0103(%r25),%r25
	copy %r20,%r24
	ldw 36(%r21),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	stw %r0,8(%r4)
L$0257: 
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldh 34(%r19),%r20
	extrs %r20,31,16,%r19
	ldw 8(%r4),%r20
	comclr,< %r20,%r19,%r0
	bl L$0258,%r0
	nop
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldo 2(%r20),%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 8(%r4),%r21
	zdep %r21,27,28,%r22
	ldw 36(%r20),%r21
	add %r22,%r21,%r20
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 8(%r4),%r22
	zdep %r22,27,28,%r23
	ldw 36(%r21),%r22
	add %r23,%r22,%r21
	ldw 4(%r21),%r22
	stw %r22,-52(%r30)
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 8(%r4),%r22
	zdep %r22,27,28,%r23
	ldw 36(%r21),%r22
	add %r23,%r22,%r21
	ldw 8(%r21),%r22
	stw %r22,-56(%r30)
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 8(%r4),%r22
	zdep %r22,27,28,%r23
	ldw 36(%r21),%r22
	add %r23,%r22,%r21
	ldw 12(%r21),%r22
	stw %r22,-60(%r30)
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 8(%r4),%r22
	zdep %r22,27,28,%r23
	ldw 36(%r21),%r22
	add %r23,%r22,%r21
	ldw 12(%r21),%r22
	stw %r22,-64(%r30)
	ldo -4(%r0),%r21
	ldo -32(%r4),%r23
	add %r23,%r21,%r22
	ldw 0(%r22),%r21
	ldw 8(%r4),%r22
	zdep %r22,27,28,%r23
	ldw 36(%r21),%r22
	add %r23,%r22,%r21
	ldw 12(%r21),%r22
	comiclr,= 0,%r22,%r0
	bl L$0260,%r0
	nop
	ldil L'LC$0071,%r21
	ldo R'LC$0071(%r21),%r21
	stw %r21,-64(%r30)
L$0260: 
	copy %r19,%r26
	ldil L'LC$0104,%r25
	ldo R'LC$0104(%r25),%r25
	ldw 8(%r4),%r24
	ldw 0(%r20),%r23
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldw 8(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0261,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 8(%r4),%r20
	zdep %r20,27,28,%r21
	ldw 36(%r19),%r20
	add %r21,%r20,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 4(%r21),%r20
	ldw 8(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl recursive_dump_type,%r2
	nop
L$0261: 
L$0259: 
	ldw 8(%r4),%r19
	ldo 1(%r19),%r20
	stw %r20,8(%r4)
	bl,n L$0257,%r0
L$0258: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0105,%r25
	ldo R'LC$0105(%r25),%r25
	ldw 40(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 40(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0262,%r0
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r20),%r21
	ldo 2(%r21),%r20
	ldw 40(%r19),%r26
	copy %r20,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl recursive_dump_type,%r2
	nop
L$0262: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0106,%r25
	ldo R'LC$0106(%r25),%r25
	ldw 44(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldw 0(%r19),%r20
	ldw 0(%r20),%r19
	comiclr,<> 6,%r19,%r0
	bl L$0265,%r0
	nop
	comiclr,>= 6,%r19,%r0
	bl L$0270,%r0
	nop
	comiclr,<> 3,%r19,%r0
	bl L$0266,%r0
	nop
	bl,n L$0267,%r0
L$0270: 
	comiclr,<> 15,%r19,%r0
	bl L$0264,%r0
	nop
	bl,n L$0267,%r0
L$0264: 
L$0265: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0107,%r25
	ldo R'LC$0107(%r25),%r25
	ldw 48(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 48(%r19),%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_arg_types,%r2
	nop
	bl,n L$0263,%r0
L$0266: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0108,%r25
	ldo R'LC$0108(%r25),%r25
	ldw 48(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -8(%r0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldw 0(%r19),%r26
	ldw 0(%r20),%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl print_cplus_stuff,%r2
	nop
	bl,n L$0263,%r0
L$0267: 
	ldo -8(%r0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -4(%r0),%r20
	ldo -32(%r4),%r22
	add %r22,%r20,%r21
	ldw 0(%r21),%r20
	ldw 0(%r19),%r26
	ldil L'LC$0109,%r25
	ldo R'LC$0109(%r25),%r25
	ldw 48(%r20),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl printfi_filtered,%r2
	nop
	ldo -4(%r0),%r19
	ldo -32(%r4),%r21
	add %r21,%r19,%r20
	ldw 0(%r20),%r19
	ldw 48(%r19),%r20
	comiclr,<> 0,%r20,%r0
	bl L$0268,%r0
	nop
	ldil L'LC$0110,%r26
	ldo R'LC$0110(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
L$0268: 
	ldil L'LC$0066,%r26
	ldo R'LC$0066(%r26),%r26
	.CALL ARGW0=GR
	bl printf_filtered,%r2
	nop
	bl,n L$0263,%r0
L$0263: 
L$0229: 
	ldw 16(%r4),%r8
	ldw 20(%r4),%r7
	ldw 24(%r4),%r6
	ldw 28(%r4),%r5
	ldo 8(%r4),%r30
	ldw -28(%r30),%r2
	bv %r0(%r2)
	ldwm -8(%r30),%r4
	.EXIT
	.PROCEND
