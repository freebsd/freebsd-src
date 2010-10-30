#name: FRV TLS relocs with addends, dynamic linking, relaxing
#source: tls-2.s
#objdump: -DR -j .text -j .got -j .plt
#ld: tmpdir/tls-1-dep.so --relax

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<_start>:
[0-9a-f ]+:	92 fc f8 11 	setlos 0xf*fffff811,gr9
[0-9a-f ]+:	92 fc 08 11 	setlos 0x811,gr9
[0-9a-f ]+:	92 c8 f0 2c 	ldi @\(gr15,44\),gr9
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
[0-9a-f ]+:	92 c8 f0 14 	ldi @\(gr15,20\),gr9
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
[0-9a-f ]+:	92 c8 f0 24 	ldi @\(gr15,36\),gr9
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
[0-9a-f ]+:	92 c8 f0 18 	ldi @\(gr15,24\),gr9
[0-9a-f ]+:	92 c8 f0 1c 	ldi @\(gr15,28\),gr9
[0-9a-f ]+:	92 c8 f0 28 	ldi @\(gr15,40\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 38 	ldi @\(gr15,56\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 34 	ldi @\(gr15,52\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 30 	ldi @\(gr15,48\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 0c 	ldi\.p @\(gr15,12\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 10 	ldi\.p @\(gr15,16\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 20 	ldi\.p @\(gr15,32\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
Disassembly of section \.got:

[0-9a-f ]+<(__data_start|_GLOBAL_OFFSET_TABLE_)>:
	\.\.\.
[0-9a-f ]+:	00 00 00 03 	add\.p gr0,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 03 	add\.p sp,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 f8 21 	\*unknown\*
[0-9a-f ]+:	00 00 00 01 	add\.p gr0,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 01 	add\.p sp,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 03 	add\.p gr16,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 01 	add\.p gr16,sp,gr0
[0-9a-f ]+:	00 01 00 01 	add\.p gr16,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 f8 11 	\*unknown\*
[0-9a-f ]+:	00 01 00 02 	add\.p gr16,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 02 	add\.p sp,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
