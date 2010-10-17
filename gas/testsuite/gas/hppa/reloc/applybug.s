	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.data

	.align 4
tab___2:
	.word L$0002
	.word L$0003
	.word L$0004
	.code

	.align 4
	.EXPORT execute,CODE
	.EXPORT execute,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
execute:
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	addil L'buf-$global$,%r27
	ldo R'buf-$global$(%r1),%r20
	ldil L'L$0002,%r19
	movb,<> %r26,%r26,L$0002
	ldo R'L$0002(%r19),%r22
	copy %r0,%r21
	addil L'tab___2-$global$,%r27
	ldo R'tab___2-$global$(%r1),%r23
	addil L'optab-$global$,%r27
	ldo R'optab-$global$(%r1),%r20
L$0009:
	sh2add %r21,%r23,%r19
	ldh 2(%r19),%r19
	ldo 1(%r21),%r21
	sub %r19,%r22,%r19
	comib,>= 2,%r21,L$0009
	sths,ma %r19,2(%r20)
	bv,n %r0(%r2)
L$0002:
	ldi 120,%r19
	stbs,ma %r19,1(%r20)
	ldhs,ma 2(%r26),%r19
	add %r22,%r19,%r19
	bv,n %r0(%r19)
L$0003:
	ldi 121,%r19
	stbs,ma %r19,1(%r20)
	ldhs,ma 2(%r26),%r19
	add %r22,%r19,%r19
	bv,n %r0(%r19)
L$0004:
	ldi 122,%r19
	stb %r19,0(%r20)
	bv %r0(%r2)
	stbs,mb %r0,1(%r20)
	.EXIT
	.PROCEND
	.IMPORT __main,CODE
	.IMPORT strcmp,CODE

	.align 4
L$C0000:
	.STRING "xyxyz\x00"
	.IMPORT abort,CODE
	.IMPORT exit,CODE
	.code

	.align 4
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(%r30)
	.CALL 
	bl __main,%r2
	ldo 128(%r30),%r30
	.CALL ARGW0=GR
	bl execute,%r2
	copy %r0,%r26
	addil L'optab-$global$,%r27
	copy %r1,%r19
	ldo R'optab-$global$(%r19),%r21
	ldh 2(%r21),%r20
	ldh R'optab-$global$(%r19),%r19
	addil L'p-$global$,%r27
	copy %r1,%r22
	sth %r20,R'p-$global$(%r22)
	ldo R'p-$global$(%r22),%r26
	sth %r20,4(%r26)
	sth %r19,2(%r26)
	ldh 4(%r21),%r19
	.CALL ARGW0=GR
	bl execute,%r2
	sth %r19,6(%r26)
	addil L'buf-$global$,%r27
	copy %r1,%r19
	ldo R'buf-$global$(%r19),%r26
	ldil L'L$C0000,%r25
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2
	ldo R'L$C0000(%r25),%r25
	comib,=,n 0,%r28,L$0011
	.CALL 
	bl abort,%r2
	nop
L$0011:
	.CALL ARGW0=GR
	bl exit,%r2
	copy %r0,%r26
	nop
	.EXIT
	.PROCEND
	.data

optab:	.comm 10
buf:	.comm 10
p:	.comm 10
