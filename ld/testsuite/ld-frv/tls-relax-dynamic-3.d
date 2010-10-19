#name: FRV TLS undefweak relocs, dynamic linking with relaxation
#source: tls-3.s
#objdump: -DR -j .text -j .got -j .plt
#ld: tmpdir/tls-1-dep.so --relax

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<_start>:
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 c8 f0 0c 	ldi\.p @\(gr15,12\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 c8 f0 0c 	ldi @\(gr15,12\),gr9
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f	 ]+: R_FRV_TLSOFF	u
