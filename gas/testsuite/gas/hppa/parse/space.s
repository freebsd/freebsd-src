	.code
	.align 4
	.export $$mulI, millicode
	.proc
	.callinfo millicode
$$mulI:
	.procend

	.code

	.align 4
	.PARAM foo, RTNVAL=GR
foo:
	.PROC
	.CALLINFO FRAME=128, NO_CALLS, ENTRY_GR=3,  ENTRY_FR=12
	.ENTRY
	bv,n %r0(%r2)
	.EXIT
	.PROCEND

	.align 4
	.import yabba, code

  	ble	R%yabba(%sr4, 	%r0)
