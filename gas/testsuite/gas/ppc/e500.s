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
	
	# Double-precision opcodes.
	efscfd 5,4
	efdabs 5,4
	efdnabs 5,4
	efdneg 5,4
	efdadd 5,4,3
	efdsub 5,4,3
	efdmul 5,4,3
	efddiv 5,4,3
	efdcmpgt 5,4,3
	efdcmplt 5,4,3
	efdcmpeq 5,4,3
	efdtstgt 5,4,3
	efdtstgt 5,4,3
	efdtstlt 5,4,3
	efdtsteq 5,4,3
	efdcfsi 5,4
	efdcfsid 5,4
	efdcfui 5,4
	efdcfuid 5,4
	efdcfsf 5,4
	efdcfuf 5,4
	efdctsi 5,4
	efdctsidz 5,4
	efdctsiz 5,4
	efdctui 5,4
	efdctuidz 5,4
	efdctuiz 5,4
	efdctsf 5,4
	efdctuf 5,4
	efdcfs 5,4
