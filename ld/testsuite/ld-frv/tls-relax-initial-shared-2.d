#name: FRV TLS relocs with addends, shared linking with static TLS, relaxing
#source: tls-2.s
#as: --defsym static_tls=1
#objdump: -DR -j .text -j .got -j .plt
#ld: -shared tmpdir/tls-1-dep.so --version-script tls-1-shared.lds --relax

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<_start>:
[0-9a-f ]+:	92 c8 f0 34 	ldi @\(gr15,52\),gr9
[0-9a-f ]+:	92 c8 f0 44 	ldi @\(gr15,68\),gr9
[0-9a-f ]+:	92 c8 f0 5c 	ldi @\(gr15,92\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 7c 	ldi @\(gr15,124\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 84 	ldi @\(gr15,132\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 94 	ldi @\(gr15,148\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 38 	ldi\.p @\(gr15,56\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 48 	ldi\.p @\(gr15,72\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 60 	ldi\.p @\(gr15,96\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 14 	setlos 0xf*fffff814,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 14 	setlos 0x814,gr9
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 14 	setlo 0xf814,gr9
[0-9a-f ]+:	92 c8 f0 64 	ldi @\(gr15,100\),gr9
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
[0-9a-f ]+:	92 c8 f0 1c 	ldi @\(gr15,28\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 98 	ldi @\(gr15,152\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 6c 	ldi @\(gr15,108\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 70 	ldi @\(gr15,112\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 68 	ldi\.p @\(gr15,104\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 10 	ldi\.p @\(gr15,16\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 20 	ldi\.p @\(gr15,32\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 24 	setlos 0xf*fffff824,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 08 24 	setlos 0x824,gr9
[0-9a-f ]+:	92 f8 00 00 	sethi hi\(0x0\),gr9
[0-9a-f ]+:	92 f4 f8 24 	setlo 0xf824,gr9
[0-9a-f ]+:	92 c8 f0 28 	ldi @\(gr15,40\),gr9
[0-9a-f ]+:	92 c8 f0 4c 	ldi @\(gr15,76\),gr9
[0-9a-f ]+:	92 c8 f0 50 	ldi @\(gr15,80\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 74 	ldi @\(gr15,116\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 88 	ldi @\(gr15,136\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 8c 	ldi @\(gr15,140\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 2c 	ldi\.p @\(gr15,44\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 3c 	ldi\.p @\(gr15,60\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 54 	ldi\.p @\(gr15,84\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 04 	setlos 0x4,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 10 04 	setlos 0x1004,gr9
[0-9a-f ]+:	92 f8 00 01 	sethi 0x1,gr9
[0-9a-f ]+:	92 f4 00 04 	setlo 0x4,gr9
[0-9a-f ]+:	92 c8 f0 30 	ldi @\(gr15,48\),gr9
[0-9a-f ]+:	92 c8 f0 40 	ldi @\(gr15,64\),gr9
[0-9a-f ]+:	92 c8 f0 58 	ldi @\(gr15,88\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 78 	ldi @\(gr15,120\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 80 	ldi @\(gr15,128\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 90 	ldi @\(gr15,144\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 14 	ldi\.p @\(gr15,20\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 18 	ldi\.p @\(gr15,24\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 24 	ldi\.p @\(gr15,36\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 34 	ldi @\(gr15,52\),gr9
[0-9a-f ]+:	92 c8 f0 5c 	ldi @\(gr15,92\),gr9
[0-9a-f ]+:	92 c8 f0 64 	ldi @\(gr15,100\),gr9
[0-9a-f ]+:	92 c8 f0 1c 	ldi @\(gr15,28\),gr9
[0-9a-f ]+:	92 c8 f0 28 	ldi @\(gr15,40\),gr9
[0-9a-f ]+:	92 c8 f0 50 	ldi @\(gr15,80\),gr9
[0-9a-f ]+:	92 c8 f0 30 	ldi @\(gr15,48\),gr9
[0-9a-f ]+:	92 c8 f0 58 	ldi @\(gr15,88\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 44 	ldi @\(gr15,68\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 00 10 11 	add\.p sp,gr17,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 10 13 	add\.p sp,gr19,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 03 	add\.p gr0,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 03 	add\.p sp,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 11 	add\.p gr16,gr17,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 13 	add\.p gr16,gr19,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 03 	add\.p gr16,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 07 f1 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 07 f3 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 01 	add\.p gr0,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 00 01 	add\.p gr0,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 03 	add\.p gr0,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 17 f3 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 10 01 	add\.p sp,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 01 	add\.p sp,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 10 03 	add\.p sp,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 17 f1 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 07 f1 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 07 f3 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 01 	add\.p gr16,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 01 	add\.p gr16,sp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 03 	add\.p gr16,gr3,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 11 	add\.p gr0,gr17,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 13 	add\.p gr0,gr19,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 10 12 	add\.p sp,gr18,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 12 	add\.p gr16,gr18,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 07 f2 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 10 02 	add\.p sp,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 00 10 02 	add\.p sp,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 17 f2 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 07 f2 	\*unknown\*
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 01 00 02 	add\.p gr16,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	x
[0-9a-f ]+:	00 01 00 02 	add\.p gr16,fp,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
[0-9a-f ]+:	00 00 00 12 	add\.p gr0,gr18,gr0
[0-9a-f	 ]+: R_FRV_TLSOFF	\.tbss
