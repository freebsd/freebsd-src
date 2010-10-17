	.text
	movb (8,a2),d1
	movb (256,a2),d1
	movb (131071,a2),d1
	movb (d2,a2),d3
	movb (131071),d2
	movb d1,(a2)
	movb d1,(8,a2)
	movb d1,(256,a2)
	movb d1,(131071,a2)
	movb d1,(d2,a2)
	movb d1,(256)
	movb d1,(131071)
