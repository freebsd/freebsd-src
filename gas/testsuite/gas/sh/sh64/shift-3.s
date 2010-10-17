! This should not get an internal error and it should emit the expected
! relocs, even though the symbol looks local and is undefined.
	.text
	.mode SHmedia
start:
	movi	.LC0 & 65535, r1
	movi	(.LC0 >> 16) & 65535, r3
