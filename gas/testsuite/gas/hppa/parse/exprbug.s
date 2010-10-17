	.code

	.align	8
	.export	icode,data
icode:
	.proc
	.callinfo	frame=0,no_calls
	.entry
	bv,n	%r0(%r2)
	.exit
	nop
	.procend

	;
	; FIRST, argv array of pointers to args, 1st is same as path.
	;
	.align	8
ic_argv:
	.word	ic_argv1-icode	; second, pointer to 1st argument
	.word	ic_path-icode		; first,  pointer to init path
	.word	0			; fourth, NULL argv terminator (pad)
	.word	0			; third,  NULL argv terminator

ic_path:
	.blockz	4096			; must be multiple of 4 bytes
	.word	0			; in case full string is used
	.word	0			; this will be the string terminator

ic_argv1:
	.blockz	4096			; must be multiple of 4 bytes
	.word	0			; in case full string is used
	.word	0			; this will be the string terminator

	.export	szicode,data
szicode:
	.word	szicode-icode
	.word	0			; must have at least one filler at end

