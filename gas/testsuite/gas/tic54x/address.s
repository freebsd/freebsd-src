	;; test all addressing permutations
	.text
_addressing:	
	and	Y,a		; direct
	and	*ar1,a		; indirect (all modes)
	and	*ar1-,b
	and	*ar1+,a
	stl	b,*+ar1
	and	*ar1-0b,a
	and	*ar1-0,b	
	and	*ar1+0,a	
	and	*ar1+0b,b	
	and	*ar1-%,a	
	and	*ar1-0%,b	
	and	*ar1+%,a	
	and	*ar1+0%,b	
	and	*ar1(32768),a	
	and	*+ar1(X+1),b
	and	*+ar1(Y)%,a
	and	*(65535),b	
	.data
X:	.word	0	
Y:	.word	1	
	.end
