asdf	reg	a0-a2/d2-d7
fdsa	equ	$16000

	movem.l	(sp)+,a0-a2/d2-d7
	movem.l	(sp)+,asdf
	
	movem.l a0-a2/d2-d7,symbol
	movem.l	asdf,symbol

	movem.l symbol,a0-a2/d2-d7
	movem.l	symbol,asdf

	movem.l fdsa,a0-a2/d2-d7
	movem.l	fdsa,asdf

	movem.l a0-a2/d2-d7,fdsa
	movem.l	asdf,fdsa
