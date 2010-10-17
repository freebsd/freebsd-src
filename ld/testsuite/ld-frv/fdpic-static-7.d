#name: FRV uClinux PIC relocs to local symbols with addends, static linking
#source: fdpic7.s
#objdump: -D
#as: -mfdpic
#ld: -static

.*:     file format elf.*frv.*

Disassembly of section \.text:

00010094 <F7>:
   10094:	80 3c 00 02 	call 1009c <\.F0\+0x4>

00010098 <\.F0>:
   10098:	80 40 f0 0c 	addi gr15,12,gr0
   1009c:	80 fc 00 0c 	setlos 0xc,gr0
   100a0:	80 f4 00 0c 	setlo 0xc,gr0
   100a4:	80 f8 00 00 	sethi hi\(0x0\),gr0
   100a8:	80 40 f0 10 	addi gr15,16,gr0
   100ac:	80 fc 00 10 	setlos 0x10,gr0
   100b0:	80 f4 00 10 	setlo 0x10,gr0
   100b4:	80 f8 00 00 	sethi hi\(0x0\),gr0
   100b8:	80 40 ff f8 	addi gr15,-8,gr0
   100bc:	80 fc ff f8 	setlos 0xfffffff8,gr0
   100c0:	80 f4 ff f8 	setlo 0xfff8,gr0
   100c4:	80 f8 ff ff 	sethi 0xffff,gr0
   100c8:	80 40 ff f4 	addi gr15,-12,gr0
   100cc:	80 fc ff f4 	setlos 0xfffffff4,gr0
   100d0:	80 f4 ff f4 	setlo 0xfff4,gr0
   100d4:	80 f8 ff ff 	sethi 0xffff,gr0
   100d8:	80 f4 00 14 	setlo 0x14,gr0
   100dc:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

000100e0 <__ROFIXUP_LIST__>:
   100e0:	00 01 41 24 	sub\.p gr20,gr36,gr0
   100e4:	00 01 41 28 	sub\.p gr20,gr40,gr0
   100e8:	00 01 41 10 	sub\.p gr20,gr16,gr0
   100ec:	00 01 41 14 	sub\.p gr20,gr20,gr0
   100f0:	00 01 41 2c 	sub\.p gr20,gr44,gr0
   100f4:	00 01 41 04 	sub\.p gr20,gr4,gr0
   100f8:	00 01 41 08 	sub\.p gr20,gr8,gr0
   100fc:	00 01 41 0c 	sub\.p gr20,gr12,gr0
   10100:	00 01 41 18 	sub\.p gr20,gr24,gr0
Disassembly of section \.data:

00014104 <D7>:
   14104:	00 01 41 0c 	sub\.p gr20,gr12,gr0

00014108 <\.D0>:
   14108:	00 01 41 10 	sub\.p gr20,gr16,gr0
   1410c:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
Disassembly of section \.got:

00014110 <_GLOBAL_OFFSET_TABLE_-0x8>:
   14110:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
   14114:	00 01 41 18 	sub\.p gr20,gr24,gr0

00014118 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
   14124:	00 01 00 9c 	addx\.p gr16,gr28,gr0,icc0
   14128:	00 01 41 10 	sub\.p gr20,gr16,gr0
   1412c:	00 01 41 0c 	sub\.p gr20,gr12,gr0
