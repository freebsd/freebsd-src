#name: FRV TLS relocs, static linking
#source: tls-1.s
#objdump: -D -j .text -j .got -j .plt
#ld: -static tmpdir/tls-1-dep.o

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<_start>:
[0-9a-f ]+:	92 fc f8 10 	setlos 0xf*fffff810,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 10 	setlos 0xf*fffff810,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc f8 10 	setlos\.p 0xf*fffff810,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 20 	setlos 0xf*fffff820,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 20 	setlos 0xf*fffff820,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc f8 20 	setlos\.p 0xf*fffff820,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 30 	setlos 0xf*fffff830,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 30 	setlos 0xf*fffff830,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc f8 30 	setlos\.p 0xf*fffff830,gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 00 	setlos lo\(0x0\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 00 	setlos lo\(0x0\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	12 fc 00 00 	setlos\.p lo\(0x0\),gr9
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	90 fc f8 30 	setlos 0xf*fffff830,gr8
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	92 fc f8 20 	setlos 0xf*fffff820,gr9
[0-9a-f ]+:	92 fc f8 10 	setlos 0xf*fffff810,gr9
[0-9a-f ]+:	92 fc f8 20 	setlos 0xf*fffff820,gr9
[0-9a-f ]+:	92 fc f8 30 	setlos 0xf*fffff830,gr9
[0-9a-f ]+:	92 fc 00 00 	setlos lo\(0x0\),gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 10 	setlos 0xf*fffff810,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 20 	setlos 0xf*fffff820,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc f8 30 	setlos 0xf*fffff830,gr9
[0-9a-f ]+:	00 88 00 00 	nop\.p
[0-9a-f ]+:	80 88 00 00 	nop
[0-9a-f ]+:	92 fc 00 00 	setlos lo\(0x0\),gr9
Disassembly of section \.got:

[0-9a-f ]+<(__data_start|_GLOBAL_OFFSET_TABLE_)>:
	\.\.\.
[0-9a-f ]+:	ff ff f8 30 	cop2 -32,cpr63,cpr48,cpr63
[0-9a-f ]+:	ff ff f8 10 	cop2 -32,cpr63,cpr16,cpr63
[0-9a-f ]+:	ff ff f8 20 	cop2 -32,cpr63,cpr32,cpr63
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
