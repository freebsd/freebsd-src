# sparc64 set insn handling (includes set, setuw, setsw, setx)

foo:
	set foo,%g2
	set 0x76543210,%g3
	set 0,%g4
	set 65535,%g5

	setx foo,%g1,%g2

	setx -1,%g1,%g3
	setx 0,%g1,%g3
	setx 1,%g1,%g3
	setx 4095,%g1,%g3
	setx 4096,%g1,%g3
	setx -4096,%g1,%g3
	setx -4097,%g1,%g3
	setx 65535,%g1,%g3
	setx -65536,%g1,%g3

	setx 2147483647,%g1,%g4
	setx 2147483648,%g1,%g4
	setx -2147483648,%g1,%g4
	setx -2147483649,%g1,%g4
	setx 4294967295,%g1,%g4
	setx 4294967296,%g1,%g4

! GAS doesn't handle large base10 numbers yet.
!	setx 9223372036854775807,%g1,%g5
!	setx 9223372036854775808,%g1,%g5
!	setx -9223372036854775808,%g1,%g5
!	setx -9223372036854775809,%g1,%g5

	setx 0x7fffffffffffffff,%g1,%g5
	setx 0x8000000000000000,%g1,%g5		! test only hh22 needed
	setx 0xffffffff00000000,%g1,%g5		! test only hm10 needed
	setx 0xffffffff80000000,%g1,%g5		! test sign-ext of lower 32
	setx 0xffff0000ffff0000,%g1,%g5		! test hh22,hi22
	setx 0xffff000000000001,%g1,%g5		! test hh22,lo10
	setx 0x00000001ffff0001,%g1,%g5		! test hm10,hi22,lo10
	setx 0x00000001ffff0000,%g1,%g5		! test hm10,hi22
	setx 0x0000000100000001,%g1,%g5		! test hm10,lo10

	setuw foo,%g2
	setuw 0x76543210,%g3
	setuw 0,%g4
	setuw 65535,%g5

	setsw foo,%g2
	setsw 0x76543210,%g3
	setsw 0,%g4
	setsw 65535,%g5
	setsw 0xffffffff,%g1
	setsw 0x7fffffff,%g2
	setsw 0xffff0000,%g3
	setsw -1,%g4
