	.SPACE $TEXT$

	.align 4
	.EXPORT mpn_add_n
	.EXPORT mpn_add_n,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR
mpn_add_n:
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY

	add	%r0,%r0,%r0		; reset cy
Loop:
	ldws,ma	 4(0,%r25),%r20
	ldws,ma	 4(0,%r24),%r19

	addc	 %r19,%r20,%r19
	addib,<> -1,%r23,Loop
	stws,ma	 %r19,4(0,%r26)

	bv	0(2)
	 addc	%r0,%r0,%r28
