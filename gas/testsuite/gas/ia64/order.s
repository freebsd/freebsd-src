	.global foo#
	.section .foo,"aw","progbits"
	.msb
	data2 0x1234
	data4 0x12345678
	data8 foo#
	.section .bar,"aw","progbits"
	.lsb
	real4 0.1
	real8 0.2
	data8 foo#
	.section .foo,"aw","progbits"
	data8 0x123456789abcdef
//	data16 0x123456789abcdef
	data8 foo#
	.section .bar,"aw","progbits"
	real10 0.4
	real16 0.8
	data8 foo#
	.section .foo,"aw","progbits"
	.lsb
	data2 0x1234
	data4 0x12345678
	data8 foo#
	.section .bar,"aw","progbits"
	.msb
	real4 0.1
	real8 0.2
	data8 foo#
	.section .foo,"aw","progbits"
	data8 0x123456789abcdef
//	data16 0x123456789abcdef
	data8 foo#
	.section .bar,"aw","progbits"
	real10 0.4
	real16 0.8
	data8 foo#
