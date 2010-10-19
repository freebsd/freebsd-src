/* m# test multiple weakrefs */
	.weakref Wnm1, nm1
	.weakref Wnm1, nm1

	.weakref Wum2, um2
	.weakref Wum2, um2
	.long um2

	.weakref Wwm3, wm3
	.weakref Wwm3, wm3
	.long Wwm3

/* r# weakref redefinitions, to and from */
	.weakref lr1, nr1
	.long lr1
	.set lr1, l
	.long lr1

	.long lr2
	.weakref lr2, nr2
	.set lr2, l
	.long lr2

	.set Wwr3, l
	.long Wwr3
	.weakref Wwr3, wr3
	.long Wwr3

	.set Wwr4, l
	.weakref Wwr4, wr4
	.long Wwr4

	.set Wwr5, l
	.long Wwr5
	.weakref Wwr5, wr5

	.weakref lr6, ur6
	.long lr6
	.set lr6, l
	.long ur6

	.weakref lr7, nr7
	.long lr7
lr7:
	.long lr7
