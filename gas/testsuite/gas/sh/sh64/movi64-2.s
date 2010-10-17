! Check MOVI expansion.  This one for the 64-bit ABI only.
	.text
start:
	movi  65536 << 16,r3
	movi  -32769 << 16,r3
	movi  32768 << 16,r3
	movi  32767 << 48,r3
	movi  32768 << 48,r3	! Perhaps a warning on this or the next,
	movi  -32768 << 48,r3	! for being out of range?

