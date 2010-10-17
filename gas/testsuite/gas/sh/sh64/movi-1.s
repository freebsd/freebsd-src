! Check MOVI expansion.  This one for the 32-bit subset.
	.text
start:
	movi  externalsym + 123,r3
	movi  65535,r3
	movi  65536,r3
	movi  65535 << 16,r3
	movi  32767,r3
	movi  32768,r3
	movi  32767 << 16,r3
	movi  -32768,r3
	movi  -32769,r3
	movi  -32768 << 16,r3
	movi  localsym + 73,r4
	movi  forwardsym - 42,r4
	.set forwardsym,47

	.data
localsym:
	.long 1
