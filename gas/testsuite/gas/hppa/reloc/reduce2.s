	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.SPACE $TEXT$
	.SUBSPA $LIT$

	.align 8
L$P0000
	.word 0x12345678
	.word 0x0

	.align 8
L$C0000
	.word 0x3ff00000
	.word 0x0
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.EXPORT g,ENTRY,PRIV_LEV=3,RTNVAL=FR
g
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	stw %r19,-32(%r30)
	ldw T'L$C0000(%r19),%r20
	bv %r0(%r2)
	fldds 0(%r20),%fr4
	.EXIT
	.PROCEND
	.IMPORT abort,CODE
	.IMPORT exit,CODE
	.SPACE $TEXT$
	.SUBSPA $LIT$

	.align 8
L$C0001
	.word 0x3ff00000
	.word 0x0
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP,ENTRY_GR=3
	.ENTRY
	stw %r2,-20(%r30)
	ldo 128(%r30),%r30
	stw %r19,-32(%r30)
	stw %r4,-128(%r30)

	copy %r19,%r4
	.CALL 
	bl g,%r2
	copy %r4,%r19
	copy %r4,%r19
	ldw T'L$C0001(%r19),%r20
	fldds 0(%r20),%fr8
	fcmp,dbl,= %fr4,%fr8
	ftest
	add,tr %r0,%r0,%r0
	b,n L$0003
	.CALL 
	bl abort,%r2
	nop
L$0003
	.CALL ARGW0=GR
	bl exit,%r2
	ldi 0,%r26
	nop
	.EXIT
	.PROCEND
