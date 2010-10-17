* Test loop directive handling
* .loop [well-defined expression]
* .break [well-defined expression]
* .endloop			
	.global label
	.asg	label,COEF
	.word	0		
	.eval	0,x
COEF	.loop	10
	.word	x		
	.eval	x+1,x		
	.if x == 6
	.break
	.endif
	.break	x == 6		
	.endloop		
	.word	x+1		
	.end
