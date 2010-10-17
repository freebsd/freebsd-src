	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:

	.code
	.align 4
	.PARAM foo,ARGW0=FR
foo:
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	bv,n %r0(%r2)
	.EXIT
	.PROCEND

	.align 4
LC$0000:
	.word P%foo

	.align 4
	.EXPORT bar,CODE
	.EXPORT bar,ENTRY,PRIV_LEV=3,RTNVAL=GR
bar:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	ldil L'LC$0000,%r19
	ldw R'LC$0000(%r19),%r26
	stw %r2,-20(%r30)
	.CALL ARGW0=GR
	bl foo,%r2
	ldo 128(%r30),%r30
	ldw -148(%r30),%r2
	bv %r0(%r2)
	ldo -128(%r30),%r30
	.EXIT
	.PROCEND
