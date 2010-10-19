#name: FRV uClinux PIC relocs to local symbols, static linking
#source: fdpic1.s
#objdump: -D
#ld: -static

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<F1>:
[0-9a-f ]+:	80 3c 00 01 	call [0-9a-f]+ <\.F0>

[0-9a-f ]+<\.F0>:
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 0c 	setlos 0xc,gr0
[0-9a-f ]+:	80 f4 00 0c 	setlo 0xc,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 10 	setlos 0x10,gr0
[0-9a-f ]+:	80 f4 00 10 	setlo 0x10,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff f8 	setlos 0xf*fffffff8,gr0
[0-9a-f ]+:	80 f4 ff f8 	setlo 0xfff8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff f0 	addi gr15,-16,gr0
[0-9a-f ]+:	80 fc ff f0 	setlos 0xf*fffffff0,gr0
[0-9a-f ]+:	80 f4 ff f0 	setlo 0xfff0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

[0-9a-f ]+<__ROFIXUP_LIST__>:
[0-9a-f ]+:	00 01 41 24 	sub\.p gr20,gr36,gr0
[0-9a-f ]+:	00 01 41 28 	sub\.p gr20,gr40,gr0
[0-9a-f ]+:	00 01 41 10 	sub\.p gr20,gr16,gr0
[0-9a-f ]+:	00 01 41 14 	sub\.p gr20,gr20,gr0
[0-9a-f ]+:	00 01 41 2c 	sub\.p gr20,gr44,gr0
[0-9a-f ]+:	00 01 41 04 	sub\.p gr20,gr4,gr0
[0-9a-f ]+:	00 01 41 08 	sub\.p gr20,gr8,gr0
[0-9a-f ]+:	00 01 41 0c 	sub\.p gr20,gr12,gr0
[0-9a-f ]+:	00 01 41 18 	sub\.p gr20,gr24,gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D1>:
[0-9a-f ]+:	00 01 41 08 	sub\.p gr20,gr8,gr0

[0-9a-f ]+<\.D0>:
[0-9a-f ]+:	00 01 41 10 	sub\.p gr20,gr16,gr0
[0-9a-f ]+:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x8>:
[0-9a-f ]+:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
[0-9a-f ]+:	00 01 41 18 	sub\.p gr20,gr24,gr0

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
[0-9a-f ]+:	00 01 41 10 	sub\.p gr20,gr16,gr0
[0-9a-f ]+:	00 01 41 08 	sub\.p gr20,gr8,gr0
