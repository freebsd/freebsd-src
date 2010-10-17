# Simple relocations against 8-bit extern symbols.
Main	SYNCD foo,$45,234
	NEGU $47,bar+48,localsym
	SWYM baz-2,45678
	TRIP fee-1,fie+1,foe+3
	RESUME foobar+8
localsym IS 42
