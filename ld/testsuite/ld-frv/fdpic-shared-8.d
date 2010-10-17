#name: FRV uClinux PIC relocs to (mostly) global symbols with addends, shared linking
#source: fdpic8.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -shared --version-script fdpic8min.ldv

.*:     file format elf.*frv.*

Disassembly of section \.text:

000004d4 <F8>:
 4d4:	80 3c 00 02 	call 4dc <GF1\+0x4>

000004d8 <GF1>:
 4d8:	80 40 f0 10 	addi gr15,16,gr0
 4dc:	80 fc 00 14 	setlos 0x14,gr0
 4e0:	80 f4 00 24 	setlo 0x24,gr0
 4e4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 4e8:	80 40 f0 0c 	addi gr15,12,gr0
 4ec:	80 fc 00 1c 	setlos 0x1c,gr0
 4f0:	80 f4 00 18 	setlo 0x18,gr0
 4f4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 4f8:	80 40 ff f8 	addi gr15,-8,gr0
 4fc:	80 fc ff f0 	setlos 0xfffffff0,gr0
 500:	80 f4 ff c8 	setlo 0xffc8,gr0
 504:	80 f8 ff ff 	sethi 0xffff,gr0
 508:	80 40 ff 4c 	addi gr15,-180,gr0
 50c:	80 fc ff 4c 	setlos 0xffffff4c,gr0
 510:	80 f4 ff 4c 	setlo 0xff4c,gr0
 514:	80 f8 ff ff 	sethi 0xffff,gr0
 518:	80 f4 00 20 	setlo 0x20,gr0
 51c:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

00004524 <D8>:
    4524:	00 00 00 04 	add\.p gr0,gr4,gr0
			4524: R_FRV_32	GD0

00004528 <GD0>:
    4528:	00 00 00 10 	add\.p gr0,gr16,gr0
			4528: R_FRV_32	\.got
    452c:	00 00 00 08 	add\.p gr0,gr8,gr0
			452c: R_FRV_32	\.text
Disassembly of section \.got:

000045a8 <_GLOBAL_OFFSET_TABLE_-0x38>:
    45a8:	00 00 00 08 	add\.p gr0,gr8,gr0
			45a8: R_FRV_FUNCDESC_VALUE	\.text
    45ac:	00 00 00 00 	add\.p gr0,gr0,gr0
    45b0:	00 00 00 08 	add\.p gr0,gr8,gr0
			45b0: R_FRV_FUNCDESC_VALUE	\.text
    45b4:	00 00 00 00 	add\.p gr0,gr0,gr0
    45b8:	00 00 00 08 	add\.p gr0,gr8,gr0
			45b8: R_FRV_FUNCDESC_VALUE	\.text
    45bc:	00 00 00 00 	add\.p gr0,gr0,gr0
    45c0:	00 00 00 08 	add\.p gr0,gr8,gr0
			45c0: R_FRV_FUNCDESC_VALUE	\.text
    45c4:	00 00 00 00 	add\.p gr0,gr0,gr0
    45c8:	00 00 00 08 	add\.p gr0,gr8,gr0
			45c8: R_FRV_FUNCDESC_VALUE	\.text
    45cc:	00 00 00 00 	add\.p gr0,gr0,gr0
    45d0:	00 00 00 08 	add\.p gr0,gr8,gr0
			45d0: R_FRV_FUNCDESC_VALUE	\.text
    45d4:	00 00 00 00 	add\.p gr0,gr0,gr0
    45d8:	00 00 00 08 	add\.p gr0,gr8,gr0
			45d8: R_FRV_FUNCDESC_VALUE	\.text
    45dc:	00 00 00 00 	add\.p gr0,gr0,gr0

000045e0 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
    45ec:	00 00 00 08 	add\.p gr0,gr8,gr0
			45ec: R_FRV_32	\.got
    45f0:	00 00 00 04 	add\.p gr0,gr4,gr0
			45f0: R_FRV_32	GF1
    45f4:	00 00 00 04 	add\.p gr0,gr4,gr0
			45f4: R_FRV_32	GF2
    45f8:	00 00 00 20 	add\.p gr0,gr32,gr0
			45f8: R_FRV_32	\.got
    45fc:	00 00 00 18 	add\.p gr0,gr24,gr0
			45fc: R_FRV_32	\.got
    4600:	00 00 00 04 	add\.p gr0,gr4,gr0
			4600: R_FRV_32	GD4
    4604:	00 00 00 04 	add\.p gr0,gr4,gr0
			4604: R_FRV_32	GF3
