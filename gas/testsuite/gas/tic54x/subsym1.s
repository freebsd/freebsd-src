*
* String substitution symbol recursion
*			
* Recursive substitution symbols	
	; recursion should stop at x
	.asg	"x",z
	.asg	"z",y
	.asg	"y",x
	add	x, A			; add x, A
	.end
