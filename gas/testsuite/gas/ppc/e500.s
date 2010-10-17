# Motorola PowerPC e500 tests
	.section ".text"
start:
	isel	2, 3, 4, 23
	dcblc	4, 5, 6
	dcbtls	7, 8, 9
	dcbtstls 10, 11, 12
	icbtls	13, 14, 15
	icblc	16, 17, 18
	mtpmr	201, 4
	mfpmr	5, 203
	bblels
	bbelr
	mtspefscr	8
	mfspefscr	9
