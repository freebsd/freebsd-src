#name: FRV uClinux PIC relocs to global symbols, static linking
#source: fdpic2.s
#objdump: -D
#as: -mfdpic
#ld: -static

.*:     file format elf.*frv.*

Disassembly of section \.text:

00010094 <F2>:
   10094:	80 3c 00 01 	call 10098 <GF0>

00010098 <GF0>:
   10098:	80 40 f0 10 	addi gr15,16,gr0
   1009c:	80 fc 00 24 	setlos 0x24,gr0
   100a0:	80 f4 00 20 	setlo 0x20,gr0
   100a4:	80 f8 00 00 	sethi hi\(0x0\),gr0
   100a8:	80 40 f0 0c 	addi gr15,12,gr0
   100ac:	80 fc 00 18 	setlos 0x18,gr0
   100b0:	80 f4 00 14 	setlo 0x14,gr0
   100b4:	80 f8 00 00 	sethi hi\(0x0\),gr0
   100b8:	80 40 ff f8 	addi gr15,-8,gr0
   100bc:	80 fc ff d0 	setlos 0xffffffd0,gr0
   100c0:	80 f4 ff c8 	setlo 0xffc8,gr0
   100c4:	80 f8 ff ff 	sethi 0xffff,gr0
   100c8:	80 40 ff c0 	addi gr15,-64,gr0
   100cc:	80 fc ff c0 	setlos 0xffffffc0,gr0
   100d0:	80 f4 ff c0 	setlo 0xffc0,gr0
   100d4:	80 f8 ff ff 	sethi 0xffff,gr0
   100d8:	80 f4 00 1c 	setlo 0x1c,gr0
   100dc:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

000100e0 <__ROFIXUP_LIST__>:
   100e0:	00 01 41 98 	subx\.p gr20,gr24,gr0,icc0
   100e4:	00 01 41 ac 	subx\.p gr20,gr44,gr0,icc0
   100e8:	00 01 41 a8 	subx\.p gr20,gr40,gr0,icc0
   100ec:	00 01 41 94 	subx\.p gr20,gr20,gr0,icc0
   100f0:	00 01 41 60 	subcc\.p gr20,gr32,gr0,icc0
   100f4:	00 01 41 64 	subcc\.p gr20,gr36,gr0,icc0
   100f8:	00 01 41 a0 	subx\.p gr20,gr32,gr0,icc0
   100fc:	00 01 41 70 	subcc\.p gr20,gr48,gr0,icc0
   10100:	00 01 41 74 	subcc\.p gr20,gr52,gr0,icc0
   10104:	00 01 41 9c 	subx\.p gr20,gr28,gr0,icc0
   10108:	00 01 41 78 	subcc\.p gr20,gr56,gr0,icc0
   1010c:	00 01 41 7c 	subcc\.p gr20,gr60,gr0,icc0
   10110:	00 01 41 80 	subx\.p gr20,gr0,gr0,icc0
   10114:	00 01 41 84 	subx\.p gr20,gr4,gr0,icc0
   10118:	00 01 41 58 	subcc\.p gr20,gr24,gr0,icc0
   1011c:	00 01 41 5c 	subcc\.p gr20,gr28,gr0,icc0
   10120:	00 01 41 50 	subcc\.p gr20,gr16,gr0,icc0
   10124:	00 01 41 54 	subcc\.p gr20,gr20,gr0,icc0
   10128:	00 01 41 a4 	subx\.p gr20,gr36,gr0,icc0
   1012c:	00 01 41 44 	subcc\.p gr20,gr4,gr0,icc0
   10130:	00 01 41 68 	subcc\.p gr20,gr40,gr0,icc0
   10134:	00 01 41 6c 	subcc\.p gr20,gr44,gr0,icc0
   10138:	00 01 41 48 	subcc\.p gr20,gr8,gr0,icc0
   1013c:	00 01 41 4c 	subcc\.p gr20,gr12,gr0,icc0
   10140:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
Disassembly of section \.data:

00014144 <D2>:
   14144:	00 01 41 48 	subcc\.p gr20,gr8,gr0,icc0

00014148 <GD0>:
   14148:	00 01 41 68 	subcc\.p gr20,gr40,gr0,icc0
   1414c:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
Disassembly of section \.got:

00014150 <_GLOBAL_OFFSET_TABLE_-0x38>:
   14150:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   14154:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14158:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   1415c:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14160:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   14164:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14168:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   1416c:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14170:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   14174:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14178:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   1417c:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0
   14180:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   14184:	00 01 41 88 	subx\.p gr20,gr8,gr0,icc0

00014188 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
   14194:	00 01 41 60 	subcc\.p gr20,gr32,gr0,icc0
   14198:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   1419c:	00 01 41 78 	subcc\.p gr20,gr56,gr0,icc0
   141a0:	00 01 41 70 	subcc\.p gr20,gr48,gr0,icc0
   141a4:	00 01 41 48 	subcc\.p gr20,gr8,gr0,icc0
   141a8:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
   141ac:	00 01 00 98 	addx\.p gr16,gr24,gr0,icc0
