	.code
	.align 4
	.import	_seterrno
 	.export   vfork  !	.label   vfork  !	.proc!	.callinfo no_calls!	.entry !	 	.label	__vfork  !	mtsp	%r0,%sr0!	ldil	L%0xc0000004,%r1!	ble	R%0xc0000004(%sr0,%r1)!	ldi	66   ,%r22 !	b,n	yyy!	b,n	__vfork !	b	_seterrno!	copy	%r28,%r26!	.label	yyy 
	add,=	%r0,%r29,%r0		 
	copy	%r0,%r28		 
	bv,n	(%r2)
	.exit
 	.procend 	
