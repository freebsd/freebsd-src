
	| Test conversion of mode 5 addressing with a zero offset into mode 2.
	.text
	move.l 0(%a3),%d1
	move.l %d2,0(%a4)
	move.l 0(%a5),0(%a1)
	.p2align 4
