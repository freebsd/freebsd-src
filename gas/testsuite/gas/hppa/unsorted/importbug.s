	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.EXPORT foo,DATA
	.data

	.align 4
foo:
	.word 0
	.IMPORT __main,CODE

	.code
	.align 4
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=64,CALLS,SAVE_RP,SAVE_SP,ENTRY_GR=3
	.ENTRY
	.import foo
	stw %r2,-20(0,%r30)
	copy %r3,%r1
	copy %r30,%r3
	stwm %r1,64(%r30)
	.CALL 
	bl __main,%r2
	nop
L$0001:
	ldw -20(%r3),%r2
	ldo 64(%r3),%r30
	ldwm -64(%r30),%r3
	bv,n %r0(%r2)
	.EXIT
	.PROCEND
