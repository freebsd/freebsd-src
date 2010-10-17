	.text
loop2:	
	move.l	%d1,%a0@+
	dbf	%d0,loop1
	.data
loop1:	bra	loop2
