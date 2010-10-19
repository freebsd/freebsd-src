#name: FRV TLS relocs with addends, dynamic linking
#source: tls-2.s
#objdump: -DR -j .text -j .got -j .plt
#ld: tmpdir/tls-1-dep.so

.*:     file format elf.*frv.*

Disassembly of section \.plt:

[0-9a-f ]+<\.plt>:
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc 08 21 	setlos 0x821,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	12 f8 00 00 	sethi\.p hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 21 	setlo 0xf821,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc 00 01 	setlos 0x1,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 c8 ff bc 	ldi @\(gr15,-68\),gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc f8 11 	setlos 0xf*fffff811,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc 10 01 	setlos 0x1001,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 c8 ff d4 	ldi @\(gr15,-44\),gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc 08 11 	setlos 0x811,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	12 f8 00 01 	sethi\.p 0x1,gr9
[0-9a-f ]+:	92 f4 00 01 	setlo 0x1,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 c8 ff ec 	ldi @\(gr15,-20\),gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	12 f8 00 00 	sethi\.p hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 11 	setlo 0xf811,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
[0-9a-f ]+:	92 fc f8 21 	setlos 0xf*fffff821,gr9
[0-9a-f ]+:	c0 3a 40 00 	bralr
Disassembly of section \.text:

[0-9a-f ]+<_start>:
[0-9a-f ]+:	92 fc f8 11 	setlos 0xf*fffff811,gr9
[0-9a-f ]+:	92 fc 08 11 	setlos 0x811,gr9
[0-9a-f ]+:	92 c8 ff f4 	ldi @\(gr15,-12\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 12 	setlos 0xf*fffff812,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 12 	setlos 0x812,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 12 	setlo 0xf812,gr9
[0-9a-f ]+:	12 fc f8 13 	setlos\.p 0xf*fffff813,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc 08 13 	setlos\.p 0x813,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 f8 00 00 	sethi\.p hi\(0x0\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f4 f8 13 	setlo 0xf813,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 14 	setlos 0xf*fffff814,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 14 	setlos 0x814,gr9
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 14 	setlo 0xf814,gr9
[0-9a-f ]+:	92 fc f8 21 	setlos 0xf*fffff821,gr9
[0-9a-f ]+:	92 fc 08 21 	setlos 0x821,gr9
[0-9a-f ]+:	92 c8 ff ac 	ldi @\(gr15,-84\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 22 	setlos 0xf*fffff822,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 22 	setlos 0x822,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 22 	setlo 0xf822,gr9
[0-9a-f ]+:	12 fc f8 23 	setlos\.p 0xf*fffff823,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc 08 23 	setlos\.p 0x823,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 f8 00 00 	sethi\.p hi\(0x0\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f4 f8 23 	setlo 0xf823,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 24 	setlos 0xf*fffff824,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 24 	setlos 0x824,gr9
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 24 	setlo 0xf824,gr9
[0-9a-f ]+:	92 fc 00 01 	setlos 0x1,gr9
[0-9a-f ]+:	92 fc 10 01 	setlos 0x1001,gr9
[0-9a-f ]+:	92 c8 ff e4 	ldi @\(gr15,-28\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 02 	setlos 0x2,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 10 02 	setlos 0x1002,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f8 00 01 	sethi 0x1,gr9
[0-9a-f ]+:	92 f4 00 02 	setlo 0x2,gr9
[0-9a-f ]+:	12 fc 00 03 	setlos\.p 0x3,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc 10 03 	setlos\.p 0x1003,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 f8 00 01 	sethi\.p 0x1,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 f4 00 03 	setlo 0x3,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 04 	setlos 0x4,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 10 04 	setlos 0x1004,gr9
[0-9a-f ]+:	92 f8 00 01 	sethi 0x1,gr9
[0-9a-f ]+:	92 f4 00 04 	setlo 0x4,gr9
[0-9a-f ]+:	92 c8 ff bc 	ldi @\(gr15,-68\),gr9
[0-9a-f ]+:	92 c8 ff d4 	ldi @\(gr15,-44\),gr9
[0-9a-f ]+:	92 c8 ff ec 	ldi @\(gr15,-20\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 20 	ldi @\(gr15,32\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 1c 	ldi @\(gr15,28\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 18 	ldi @\(gr15,24\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 0c 	ldi\.p @\(gr15,12\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 10 	ldi\.p @\(gr15,16\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 14 	ldi\.p @\(gr15,20\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x60>:
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 08 21 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 f8 21 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 00 01 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_TLSDESC_VALUE	x
[0-9a-f ]+:	00 00 00 01 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	ff ff f8 11 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 10 01 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_TLSDESC_VALUE	x
[0-9a-f ]+:	00 00 10 01 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 08 11 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 01 00 01 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_TLSDESC_VALUE	x
[0-9a-f ]+:	00 01 00 01 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	00 00 f8 11 	.*
[0-9a-f ]+:	00 01 02 c0 	.*
[0-9a-f ]+:	ff ff f8 21 	.*

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 00 00 03 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 03 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 03 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 02 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 02 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 00 02 	.*
[0-9a-f	 ]+: R_FRV_TLSOFF	x
