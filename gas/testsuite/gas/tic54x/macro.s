* Macro test
	.sslist
	.text
	.global abc, def, ghi, adr
	
* Macro library; load and use a macro in macros.lib
	.mlib "macros.lib"

	IN_MLIB	abc,def,ghi

add3	.macro	P1,P2,P3,ADDRP
	ld	P1,a			
	add	P2,a			
	add	P3,a			
	stl	a,ADDRP			
	.endm
	add3	abc, def, ghi, adr
	
* Forced substitution within a macro
force	.macro	x
	.asg	0, x
	.loop 8
AUX:x:	.set	x
	.eval	x+1,x
	.endloop		
	.endm
	force
	
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
	ADDX	#100			; ADD #100,A
	.end
