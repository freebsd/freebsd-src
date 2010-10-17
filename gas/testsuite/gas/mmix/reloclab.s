# Different relocations for extern labels: GETA, PUSHJ, Bcc, JMP.
# Mix in different accesses to local labels to see that relaxing works for
# this case.
Main	JMP foo+8
	JMP here
	GETA $8,here
	BOD $99,here
	SWYM 0
here	BZ $222,bar+16
there	GETA $4,baz
	PUSHJ $7,foobar
	JMP there
	GETA $88,there
	BOD $111,there
