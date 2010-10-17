! Check that shift expressions translate to the proper reloc for MOVI and
! SHORI for local and external symbols.  This is the 64-bit subset.
	.text
	.mode SHmedia
start:
	movi (localsym >> 32) & 65535,r4
	movi (localsym >> 48) & 65535,r4

	movi ((localsym + 44) >> 32) & 65535,r4
	movi ((localsym + 43) >> 48) & 65535,r4

	movi (externsym >> 32) & 65535,r4
	movi (externsym >> 48) & 65535,r4

	movi ((externsym + 41) >> 32) & 65535,r4
	movi ((externsym + 42) >> 48) & 65535,r4

	shori (localsym >> 32) & 65535,r4
	shori (localsym >> 48) & 65535,r4

	shori ((localsym + 44) >> 32) & 65535,r4
	shori ((localsym + 43) >> 48) & 65535,r4

	shori (externsym >> 32) & 65535,r4
	shori (externsym >> 48) & 65535,r4

	shori ((externsym + 41) >> 32) & 65535,r4
	shori ((externsym + 42) >> 48) & 65535,r4

	.data
! Just make localsym have a non-zero offset into .data.
	.long 0
localsym:
	.long 0
