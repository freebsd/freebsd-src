#name: FRV uClinux PIC relocs to undefined symbols, shared linking
#source: fdpic5.s
#objdump: -DRz -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -shared

.*:     file format elf.*frv.*

Disassembly of section \.plt:

00000598 <\.plt>:
 598:	00 00 00 10 	add\.p gr0,gr16,gr0
 59c:	c0 1a 00 06 	bra 5b4 <F5-0x10>
 5a0:	00 00 00 08 	add\.p gr0,gr8,gr0
 5a4:	c0 1a 00 04 	bra 5b4 <F5-0x10>
 5a8:	00 00 00 00 	add\.p gr0,gr0,gr0
 5ac:	c0 1a 00 02 	bra 5b4 <F5-0x10>
 5b0:	00 00 00 18 	add\.p gr0,gr24,gr0
 5b4:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
 5b8:	80 30 40 00 	jmpl @\(gr4,gr0\)
 5bc:	9c cc ff f0 	lddi @\(gr15,-16\),gr14
 5c0:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

000005c4 <F5>:
 5c4:	fe 3f ff fe 	call 5bc <F5-0x8>
 5c8:	80 40 f0 0c 	addi gr15,12,gr0
 5cc:	80 fc 00 24 	setlos 0x24,gr0
 5d0:	80 f4 00 20 	setlo 0x20,gr0
 5d4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5d8:	80 40 f0 10 	addi gr15,16,gr0
 5dc:	80 fc 00 1c 	setlos 0x1c,gr0
 5e0:	80 f4 00 18 	setlo 0x18,gr0
 5e4:	80 f8 00 00 	sethi hi\(0x0\),gr0
 5e8:	80 40 ff f8 	addi gr15,-8,gr0
 5ec:	80 fc ff e8 	setlos 0xffffffe8,gr0
 5f0:	80 f4 ff e0 	setlo 0xffe0,gr0
 5f4:	80 f8 ff ff 	sethi 0xffff,gr0
 5f8:	80 f4 00 14 	setlo 0x14,gr0
 5fc:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

00004604 <D5>:
    4604:	00 00 00 00 	add\.p gr0,gr0,gr0
			4604: R_FRV_32	UD0
    4608:	00 00 00 00 	add\.p gr0,gr0,gr0
			4608: R_FRV_FUNCDESC	UFb
    460c:	00 00 00 00 	add\.p gr0,gr0,gr0
			460c: R_FRV_32	UFb
Disassembly of section \.got:

000046a0 <_GLOBAL_OFFSET_TABLE_-0x20>:
    46a0:	00 00 05 b4 	subx\.p gr0,gr52,gr0,icc1
			46a0: R_FRV_FUNCDESC_VALUE	UF9
    46a4:	00 00 00 00 	add\.p gr0,gr0,gr0
    46a8:	00 00 05 9c 	subx\.p gr0,gr28,gr0,icc1
			46a8: R_FRV_FUNCDESC_VALUE	UF8
    46ac:	00 00 00 00 	add\.p gr0,gr0,gr0
    46b0:	00 00 05 ac 	subx\.p gr0,gr44,gr0,icc1
			46b0: R_FRV_FUNCDESC_VALUE	UF0
    46b4:	00 00 00 00 	add\.p gr0,gr0,gr0
    46b8:	00 00 05 a4 	subx\.p gr0,gr36,gr0,icc1
			46b8: R_FRV_FUNCDESC_VALUE	UF7
    46bc:	00 00 00 00 	add\.p gr0,gr0,gr0

000046c0 <_GLOBAL_OFFSET_TABLE_>:
    46c0:	00 00 00 00 	add\.p gr0,gr0,gr0
    46c4:	00 00 00 00 	add\.p gr0,gr0,gr0
    46c8:	00 00 00 00 	add\.p gr0,gr0,gr0
    46cc:	00 00 00 00 	add\.p gr0,gr0,gr0
			46cc: R_FRV_32	UF1
    46d0:	00 00 00 00 	add\.p gr0,gr0,gr0
			46d0: R_FRV_FUNCDESC	UF4
    46d4:	00 00 00 00 	add\.p gr0,gr0,gr0
			46d4: R_FRV_32	UD1
    46d8:	00 00 00 00 	add\.p gr0,gr0,gr0
			46d8: R_FRV_FUNCDESC	UF6
    46dc:	00 00 00 00 	add\.p gr0,gr0,gr0
			46dc: R_FRV_FUNCDESC	UF5
    46e0:	00 00 00 00 	add\.p gr0,gr0,gr0
			46e0: R_FRV_32	UF3
    46e4:	00 00 00 00 	add\.p gr0,gr0,gr0
			46e4: R_FRV_32	UF2
