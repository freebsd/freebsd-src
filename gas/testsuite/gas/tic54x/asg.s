* asg/eval test substitution symbols
* .eval	value, symbol
* .asg "string", symbol
* .asg string, symbol
	.global L1,L2,L3,newlabel,end
	.sslist				; list line substitutions
	.text
	.asg	*ar0+, INC		; replace a complete operand
	.asg	ar0, FP			; replace a sub-operand
	.asg	"add #1,a", doit	; macro-style
	.asg	newlabel, LABEL		; replace a label
	.asg	.word 0, PSEUDO		; replace with a directive
	
L1:	add	#100,a 		
L2:	ld	*FP+,a 			
L3:	ld	INC,a 			
	.asg	0,L2			
LABEL:	add	#L2,a			
	doit				
	.asg	0, x
	.loop	5
	.eval	x+1,x			
	.word	x			
	.endloop
	PSEUDO				
	
* Tests from 5.3.2	
	.asg	AR0,FP
	.asg	*AR1+,Ind
	.asg	*AR1+0b,Rc_Prop
	.asg	"string",strng		; NOTE:	"""string""" not supported
	.asg	"a,b,c",parms
	.asg	1,counter
	.loop	100
	.word	counter
	.eval	counter + 1, counter
	.endloop
end:	.word	0x100	
	.end
