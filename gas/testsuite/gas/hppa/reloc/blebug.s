	.code
	.align 4
	.IMPORT	$$dyncall,MILLICODE ; Code for dynamic function calls.

_sigtramp:
	ldil	L%$$dyncall,%r2	;   whose address is in r22.
	ble	R%$$dyncall(%sr4,%r2)
