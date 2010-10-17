#name: FRV uClinux PIC relocs to global symbols, pie linking
#source: fdpic2.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -pie

.*:     file format elf.*frv.*

Disassembly of section \.text:

000005d0 <F2>:
 5d0:	80 3c 00 01 	call 5d4 <GF0>

000005d4 <GF0>:
 5d4:	80 40 f0 10 	addi gr15,16,gr0
 5d8:	80 fc 00 24 	setlos 0x24,gr0
 5dc:	80 f4 00 20 	setlo 0x20,gr0
 5e0:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5e4:	80 40 f0 0c 	addi gr15,12,gr0
 5e8:	80 fc 00 18 	setlos 0x18,gr0
 5ec:	80 f4 00 14 	setlo 0x14,gr0
 5f0:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5f4:	80 40 ff f8 	addi gr15,-8,gr0
 5f8:	80 fc ff f0 	setlos 0xfffffff0,gr0
 5fc:	80 f4 ff e8 	setlo 0xffe8,gr0
 600:	80 f8 ff ff 	sethi 0xffff,gr0
 604:	80 40 ff 64 	addi gr15,-156,gr0
 608:	80 fc ff 64 	setlos 0xffffff64,gr0
 60c:	80 f4 ff 64 	setlo 0xff64,gr0
 610:	80 f8 ff ff 	sethi 0xffff,gr0
 614:	80 f4 00 1c 	setlo 0x1c,gr0
 618:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

00004630 <D2>:
    4630:	00 00 00 04 	add\.p gr0,gr4,gr0
			4630: R_FRV_32	\.data

00004634 <GD0>:
    4634:	00 00 00 04 	add\.p gr0,gr4,gr0
			4634: R_FRV_FUNCDESC	\.text
    4638:	00 00 00 04 	add\.p gr0,gr4,gr0
			4638: R_FRV_32	\.text
Disassembly of section \.got:

000046b8 <_GLOBAL_OFFSET_TABLE_-0x18>:
    46b8:	00 00 00 04 	add\.p gr0,gr4,gr0
			46b8: R_FRV_FUNCDESC_VALUE	\.text
    46bc:	00 00 00 02 	add\.p gr0,fp,gr0
    46c0:	00 00 00 04 	add\.p gr0,gr4,gr0
			46c0: R_FRV_FUNCDESC_VALUE	\.text
    46c4:	00 00 00 02 	add\.p gr0,fp,gr0
    46c8:	00 00 00 04 	add\.p gr0,gr4,gr0
			46c8: R_FRV_FUNCDESC_VALUE	\.text
    46cc:	00 00 00 02 	add\.p gr0,fp,gr0

000046d0 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
    46dc:	00 00 00 04 	add\.p gr0,gr4,gr0
			46dc: R_FRV_FUNCDESC	\.text
    46e0:	00 00 00 04 	add\.p gr0,gr4,gr0
			46e0: R_FRV_32	\.text
    46e4:	00 00 00 04 	add\.p gr0,gr4,gr0
			46e4: R_FRV_FUNCDESC	\.text
    46e8:	00 00 00 04 	add\.p gr0,gr4,gr0
			46e8: R_FRV_FUNCDESC	\.text
    46ec:	00 00 00 04 	add\.p gr0,gr4,gr0
			46ec: R_FRV_32	\.data
    46f0:	00 00 00 04 	add\.p gr0,gr4,gr0
			46f0: R_FRV_32	\.text
    46f4:	00 00 00 04 	add\.p gr0,gr4,gr0
			46f4: R_FRV_32	\.text
