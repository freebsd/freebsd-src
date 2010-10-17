//	
// Detect WAR violations.  Cases taken from DV tables.
//	
.text
	.explicit
// PR63
(p63)	br.cond.sptk	b0
	br.wtop.sptk	L	
L:	
