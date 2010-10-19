	.h8300hn
	.text
foo:
	mov.l er6,@-er7
	mov.w r7,r6
	mov.w #2000,r2
	mov.w r2,@-4064:16
	mov.w #10000,r2
	mov.w r2,@_var2
	mov.l @er7+,er6
	rts
	.comm _var2,2,2
	.end
