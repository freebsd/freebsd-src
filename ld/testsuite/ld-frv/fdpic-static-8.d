#name: FRV uClinux PIC relocs to global symbols with addends, static linking
#source: fdpic8.s
#objdump: -D
#ld: -static

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<F8>:
[0-9a-f ]+:	80 3c 00 02 	call [0-9a-f]+ <GF0\+0x4>

[0-9a-f ]+<GF0>:
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 14 	setlos 0x14,gr0
[0-9a-f ]+:	80 f4 00 24 	setlo 0x24,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 1c 	setlos 0x1c,gr0
[0-9a-f ]+:	80 f4 00 18 	setlo 0x18,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff f0 	setlos 0xf*fffffff0,gr0
[0-9a-f ]+:	80 f4 ff c8 	setlo 0xffc8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff c4 	addi gr15,-60,gr0
[0-9a-f ]+:	80 fc ff c4 	setlos 0xf*ffffffc4,gr0
[0-9a-f ]+:	80 f4 ff c4 	setlo 0xffc4,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

[0-9a-f ]+<__ROFIXUP_LIST__>:
[0-9a-f ]+:	00 01 41 98 	subx\.p gr20,gr24,gr0,icc0
[0-9a-f ]+:	00 01 41 9c 	subx\.p gr20,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 ac 	subx\.p gr20,gr44,gr0,icc0
[0-9a-f ]+:	00 01 41 94 	subx\.p gr20,gr20,gr0,icc0
[0-9a-f ]+:	00 01 41 58 	subcc\.p gr20,gr24,gr0,icc0
[0-9a-f ]+:	00 01 41 5c 	subcc\.p gr20,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 a4 	subx\.p gr20,gr36,gr0,icc0
[0-9a-f ]+:	00 01 41 68 	subcc\.p gr20,gr40,gr0,icc0
[0-9a-f ]+:	00 01 41 6c 	subcc\.p gr20,gr44,gr0,icc0
[0-9a-f ]+:	00 01 41 a0 	subx\.p gr20,gr32,gr0,icc0
[0-9a-f ]+:	00 01 41 70 	subcc\.p gr20,gr48,gr0,icc0
[0-9a-f ]+:	00 01 41 74 	subcc\.p gr20,gr52,gr0,icc0
[0-9a-f ]+:	00 01 41 80 	subx\.p gr20,gr0,gr0,icc0
[0-9a-f ]+:	00 01 41 84 	subx\.p gr20,gr4,gr0,icc0
[0-9a-f ]+:	00 01 41 78 	subcc\.p gr20,gr56,gr0,icc0
[0-9a-f ]+:	00 01 41 7c 	subcc\.p gr20,gr60,gr0,icc0
[0-9a-f ]+:	00 01 41 50 	subcc\.p gr20,gr16,gr0,icc0
[0-9a-f ]+:	00 01 41 54 	subcc\.p gr20,gr20,gr0,icc0
[0-9a-f ]+:	00 01 41 a8 	subx\.p gr20,gr40,gr0,icc0
[0-9a-f ]+:	00 01 41 44 	subcc\.p gr20,gr4,gr0,icc0
[0-9a-f ]+:	00 01 41 60 	subcc\.p gr20,gr32,gr0,icc0
[0-9a-f ]+:	00 01 41 64 	subcc\.p gr20,gr36,gr0,icc0
[0-9a-f ]+:	00 01 41 48 	subcc\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 41 4c 	subcc\.p gr20,gr12,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D8>:
[0-9a-f ]+:	00 01 41 4c 	subcc\.p gr20,gr12,gr0,icc0

[0-9a-f ]+<GD0>:
[0-9a-f ]+:	00 01 41 60 	subcc\.p gr20,gr32,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x38>:
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 01 41 58 	subcc\.p gr20,gr24,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
[0-9a-f ]+:	00 01 41 70 	subcc\.p gr20,gr48,gr0,icc0
[0-9a-f ]+:	00 01 41 68 	subcc\.p gr20,gr40,gr0,icc0
[0-9a-f ]+:	00 01 41 4c 	subcc\.p gr20,gr12,gr0,icc0
[0-9a-f ]+:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
