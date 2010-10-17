# sparc64 special relocs

foo:
	sethi %uhi(0x1234567800000000),%g1
	or %g1,%ulo(0x1234567800000000),%g1
	nop
	sethi %uhi(foo),%g1
	or %g1,%ulo(foo),%g1
	nop
	sethi %uhi(foo+0x1234567800000000),%g1
	or %g1,%ulo(foo+0x1234567800000000),%g1
	nop
	sethi %hh(0xfedcba9876543210),%g1
	or %g1,%hm(0xfedcba9876543210),%g1
	sethi %lm(0xfedcba9876543210),%g2
	or %g1,%lo(0xfedcba9876543210),%g2
	nop
	sethi %hh(foo),%g1
	or %g1,%hm(foo),%g1
	sethi %lm(foo),%g2
	or %g1,%lo(foo),%g2
	nop
	sethi %hh(foo+0xfedcba9876543210),%g1
	or %g1,%hm(foo+0xfedcba9876543210),%g1
	sethi %lm(foo+0xfedcba9876543210),%g2
	or %g1,%lo(foo+0xfedcba9876543210),%g2
	nop
	sethi %h44(0xa9876543210),%g1
	or %g1,%m44(0xa9876543210),%g1
	or %g1,%l44(0xa9876543210),%g1
	nop
	sethi %h44(foo),%g1
	or %g1,%m44(foo),%g1
	or %g1,%l44(foo),%g1
	nop
	sethi %h44(foo+0xa9876543210),%g1
	or %g1,%m44(foo+0xa9876543210),%g1
	or %g1,%l44(foo+0xa9876543210),%g1
	nop
	sethi %hix(0xffffffff76543210),%g1
	xor %g1,%lox(0xffffffff76543210),%g1
	nop
	sethi %hix(foo),%g1
	xor %g1,%lox(foo),%g1
	nop
	sethi %hix(foo+0xffffffff76543210),%g1
	xor %g1,%lox(foo+0xffffffff76543210),%g1
	nop
