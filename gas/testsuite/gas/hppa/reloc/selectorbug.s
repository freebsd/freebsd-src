; gcc_compiled.:
	.EXPORT intVec_error_handler,DATA
	.data

intVec_error_handler:
	.word P%default_intVec_error_handler__FPCc

	.code
	.align 4
	.EXPORT foo,CODE
	.EXPORT foo,ENTRY,PRIV_LEV=3
foo:
	.PROC
	.CALLINFO FRAME=0
	.ENTRY
	.stabd 68,0,41
	.EXIT
	.PROCEND
