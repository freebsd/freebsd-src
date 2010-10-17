! Check that shift expressions translate to the proper reloc for MOVI and
! SHORI for local and external symbols.  This is the 32-bit subset.
	.text
	.mode SHmedia
start:
	movi localsym & 65535,r4
	movi (localsym >> 0) & 65535,r4
	movi (localsym >> 16) & 65535,r4

	movi externsym & 65535,r4
	movi (externsym >> 0) & 65535,r4
	movi (externsym >> 16) & 65535,r4

	shori localsym & 65535,r4
	shori (localsym >> 0) & 65535,r4
	shori (localsym >> 16) & 65535,r4

	shori externsym & 65535,r4
	shori (externsym >> 0) & 65535,r4
	shori (externsym >> 16) & 65535,r4

	movi (localsym + 42) & 65535,r4
	movi ((localsym + 43) >> 0) & 65535,r4
	movi ((localsym + 44) >> 16) & 65535,r4

	movi (externsym + 45) & 65535,r4
	movi ((externsym + 46) >> 0) & 65535,r4
	movi ((externsym + 47) >> 16) & 65535,r4

	shori (localsym + 42) & 65535,r4
	shori ((localsym + 43) >> 0) & 65535,r4
	shori ((localsym + 44) >> 16) & 65535,r4

	shori (externsym + 45) & 65535,r4
	shori ((externsym + 46) >> 0) & 65535,r4
	shori ((externsym + 47) >> 16) & 65535,r4

	.data
! Just make localsym have a non-zero offset into .data.
	.long 0
localsym:
	.long 0
