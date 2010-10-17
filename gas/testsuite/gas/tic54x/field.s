*
* test .field directive
*	
	.global	X
	.global f1,f2,f3,f4,f5,f6,f7,f8
f1:	.field	0ABCh, 14	; f1=0x0
f2:	.field	0Ah, 5		; should align to next word, f2=0x1
f3:	.field	0Ch, 4		; should be packed in previous word, f3=0x1
f4:	.field	f3		; align at word 0x2
f5:	.field	04321h, 32	;
f6:	.field  01111b		; default to 16-bit field
f7:	.field	3,3	
f8:	.field	69,15		; align at next word
	.end
