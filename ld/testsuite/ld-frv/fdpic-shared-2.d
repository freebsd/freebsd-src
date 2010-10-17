#name: FRV uClinux PIC relocs to (mostly) global symbols, shared linking
#source: fdpic2.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -shared --version-script fdpic2min.ldv

.*:     file format elf.*frv.*

Disassembly of section \.plt:

000005c8 <\.plt>:
 5c8:	00 00 00 00 	add\.p gr0,gr0,gr0
 5cc:	c0 1a 00 06 	bra 5e4 <F2-0x10>
 5d0:	00 00 00 10 	add\.p gr0,gr16,gr0
 5d4:	c0 1a 00 04 	bra 5e4 <F2-0x10>
 5d8:	00 00 00 18 	add\.p gr0,gr24,gr0
 5dc:	c0 1a 00 02 	bra 5e4 <F2-0x10>
 5e0:	00 00 00 08 	add\.p gr0,gr8,gr0
 5e4:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
 5e8:	80 30 40 00 	jmpl @\(gr4,gr0\)
 5ec:	9c cc ff f8 	lddi @\(gr15,-8\),gr14
 5f0:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

000005f4 <F2>:
 5f4:	fe 3f ff fe 	call 5ec <F2-0x8>

000005f8 <GF0>:
 5f8:	80 40 f0 10 	addi gr15,16,gr0
 5fc:	80 fc 00 24 	setlos 0x24,gr0
 600:	80 f4 00 20 	setlo 0x20,gr0
 604:	80 f8 00 00 	sethi hi\(0x0\),gr0
 608:	80 40 f0 0c 	addi gr15,12,gr0
 60c:	80 fc 00 18 	setlos 0x18,gr0
 610:	80 f4 00 14 	setlo 0x14,gr0
 614:	80 f8 00 00 	sethi hi\(0x0\),gr0
 618:	80 40 ff f0 	addi gr15,-16,gr0
 61c:	80 fc ff e8 	setlos 0xffffffe8,gr0
 620:	80 f4 ff e0 	setlo 0xffe0,gr0
 624:	80 f8 ff ff 	sethi 0xffff,gr0
 628:	80 40 ff 48 	addi gr15,-184,gr0
 62c:	80 fc ff 48 	setlos 0xffffff48,gr0
 630:	80 f4 ff 48 	setlo 0xff48,gr0
 634:	80 f8 ff ff 	sethi 0xffff,gr0
 638:	80 f4 00 1c 	setlo 0x1c,gr0
 63c:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

00004644 <D2>:
    4644:	00 00 00 00 	add\.p gr0,gr0,gr0
			4644: R_FRV_32	GD0

00004648 <GD0>:
	\.\.\.
			4648: R_FRV_FUNCDESC	GFb
			464c: R_FRV_32	GFb
Disassembly of section \.got:

000046e0 <_GLOBAL_OFFSET_TABLE_-0x20>:
    46e0:	00 00 05 dc 	subxcc\.p gr0,gr28,gr0,icc1
			46e0: R_FRV_FUNCDESC_VALUE	GF9
    46e4:	00 00 00 00 	add\.p gr0,gr0,gr0
    46e8:	00 00 05 d4 	subxcc\.p gr0,gr20,gr0,icc1
			46e8: R_FRV_FUNCDESC_VALUE	GF8
    46ec:	00 00 00 00 	add\.p gr0,gr0,gr0
    46f0:	00 00 05 e4 	subxcc\.p gr0,gr36,gr0,icc1
			46f0: R_FRV_FUNCDESC_VALUE	GF7
    46f4:	00 00 00 00 	add\.p gr0,gr0,gr0
    46f8:	00 00 05 cc 	subxcc\.p gr0,gr12,gr0,icc1
			46f8: R_FRV_FUNCDESC_VALUE	GF0
    46fc:	00 00 00 00 	add\.p gr0,gr0,gr0

00004700 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
			470c: R_FRV_FUNCDESC	GF4
			4710: R_FRV_32	GF1
			4714: R_FRV_FUNCDESC	GF6
			4718: R_FRV_FUNCDESC	GF5
			471c: R_FRV_32	GD4
			4720: R_FRV_32	GF3
			4724: R_FRV_32	GF2
