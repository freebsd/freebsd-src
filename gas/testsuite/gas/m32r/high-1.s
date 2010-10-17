; Test high/shigh handling.

foo:
	seth r4,#high(foo+0x10000)
	or3  r4,r4,#low(foo+0x10000)

	seth r4,#high(0x12348765)
	or3  r4,r4,#low(0x12348765)

	seth r4,#shigh(0x12348765)
	or3  r4,r4,#low(0x12348765)

	seth r4,#shigh(0x87654321)
	or3  r4,r4,#low(0x87654321)
