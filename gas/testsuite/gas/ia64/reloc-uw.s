	.text

	.macro uw, type
	.proc	uw\type
	.\type	uw\type
uw\type:
	.unwentry
	br.ret.sptk rp
	.endp	uw\type
	.endm

	uw global
	uw weak
