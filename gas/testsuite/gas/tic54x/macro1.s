* Subsripted substitution symbols
ADDX	.macro	ABC
	.var	TMP
	.asg	:ABC(1):,TMP	
	.if	$symcmp(TMP,"#") == 0
	ADD	ABC,A
	.else
	.emsg	"Bad macro parameter 'ABC'"
	.endif
	.endm
	ADDX	*AR1			; should produce an error msg
	.end
