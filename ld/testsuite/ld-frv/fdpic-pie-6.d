#name: FRV uClinux PIC relocs to weak undefined symbols, pie linking
#source: fdpic6.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -pie --defsym WD1=D6

.*:     file format elf.*frv.*

Disassembly of section \.plt:

000005a8 <\.plt>:
 5a8:	00 00 00 08 	add\.p gr0,gr8,gr0
 5ac:	c0 1a 00 06 	bra 5c4 <F6-0x10>
 5b0:	00 00 00 00 	add\.p gr0,gr0,gr0
 5b4:	c0 1a 00 04 	bra 5c4 <F6-0x10>
 5b8:	00 00 00 10 	add\.p gr0,gr16,gr0
 5bc:	c0 1a 00 02 	bra 5c4 <F6-0x10>
 5c0:	00 00 00 18 	add\.p gr0,gr24,gr0
 5c4:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
 5c8:	80 30 40 00 	jmpl @\(gr4,gr0\)
 5cc:	9c cc ff f0 	lddi @\(gr15,-16\),gr14
 5d0:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

000005d4 <F6>:
 5d4:	fe 3f ff fe 	call 5cc <F6-0x8>
 5d8:	80 40 f0 0c 	addi gr15,12,gr0
 5dc:	80 fc 00 24 	setlos 0x24,gr0
 5e0:	80 f4 00 20 	setlo 0x20,gr0
 5e4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5e8:	80 40 f0 10 	addi gr15,16,gr0
 5ec:	80 fc 00 18 	setlos 0x18,gr0
 5f0:	80 f4 00 1c 	setlo 0x1c,gr0
 5f4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5f8:	80 40 ff f8 	addi gr15,-8,gr0
 5fc:	80 fc ff e8 	setlos 0xffffffe8,gr0
 600:	80 f4 ff e0 	setlo 0xffe0,gr0
 604:	80 f8 ff ff 	sethi 0xffff,gr0
 608:	80 f4 ff 44 	setlo 0xff44,gr0
 60c:	80 f8 ff ff 	sethi 0xffff,gr0
 610:	80 f4 00 14 	setlo 0x14,gr0
 614:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

0000462c <D6>:
	\.\.\.
			462c: R_FRV_32	WD0
			4630: R_FRV_FUNCDESC	WFb
			4634: R_FRV_32	WFb
Disassembly of section \.got:

000046c8 <_GLOBAL_OFFSET_TABLE_-0x20>:
    46c8:	00 00 05 c4 	subxcc\.p gr0,gr4,gr0,icc1
			46c8: R_FRV_FUNCDESC_VALUE	WF9
    46cc:	00 00 00 02 	add\.p gr0,fp,gr0
    46d0:	00 00 05 bc 	subx\.p gr0,gr60,gr0,icc1
			46d0: R_FRV_FUNCDESC_VALUE	WF8
    46d4:	00 00 00 02 	add\.p gr0,fp,gr0
    46d8:	00 00 05 b4 	subx\.p gr0,gr52,gr0,icc1
			46d8: R_FRV_FUNCDESC_VALUE	WF0
    46dc:	00 00 00 02 	add\.p gr0,fp,gr0
    46e0:	00 00 05 ac 	subx\.p gr0,gr44,gr0,icc1
			46e0: R_FRV_FUNCDESC_VALUE	WF7
    46e4:	00 00 00 02 	add\.p gr0,fp,gr0

000046e8 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
			46f4: R_FRV_32	WF1
			46f8: R_FRV_FUNCDESC	WF4
			46fc: R_FRV_32	WD2
			4700: R_FRV_FUNCDESC	WF5
			4704: R_FRV_FUNCDESC	WF6
			4708: R_FRV_32	WF3
			470c: R_FRV_32	WF2
