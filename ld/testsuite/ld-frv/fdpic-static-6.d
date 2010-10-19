#name: FRV uClinux PIC relocs to weak undefined symbols, static linking
#source: fdpic6.s
#objdump: -D
#ld: -static
#warning: different segment

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<F6>:
[0-9a-f ]+:	fe 3f bf db 	call 0 <_gp-0xf8d8>
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 24 	setlos 0x24,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 18 	setlos 0x18,gr0
[0-9a-f ]+:	80 f4 00 1c 	setlo 0x1c,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff f0 	setlos 0xf*fffffff0,gr0
[0-9a-f ]+:	80 f4 ff e8 	setlo 0xffe8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 be e0 	setlo 0xbee0,gr0
[0-9a-f ]+:	80 f8 ff fe 	sethi 0xfffe,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

[0-9a-f ]+<__ROFIXUP_LIST__>:
[0-9a-f ]+:	00 01 41 20 	sub\.p gr20,gr32,gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D6>:
	\.\.\.
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x38>:
	\.\.\.

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
