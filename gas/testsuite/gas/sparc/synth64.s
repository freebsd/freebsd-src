# sparc64 synthetic insns
	.text
	iprefetch foo
foo:
	signx %g1,%g2
	clruw %g1,%g2
	cas [%g1],%g2,%g3
	casl [%g1],%g2,%g3
	casx [%g1],%g2,%g3
	casxl [%g1],%g2,%g3

	clrx [%g1+%g2]
	clrx [%g1]
	clrx [%g1+1]
	clrx [42+%g1]
	clrx [0x42]

	signx %g1
	clruw %g2
