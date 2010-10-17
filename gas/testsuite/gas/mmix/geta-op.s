# Simple GETA/BRANCH/PUSHJ operands.
Main	SWYM 0,0,0
here	SWYM 0,0,0
	GETA $25,here
at	GETA $32,at
	BZ $78,there
	PUSHJ X0,here
	PUSHJ X,here
	GETA X,there
	PUSHJ X,there
	PUSHJ $73,there
	PUSHJ 56,there
	PBEV X,here
there	SWYM 0,0,0
X IS $135
X0 IS 91
