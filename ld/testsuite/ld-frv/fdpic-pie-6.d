#name: FRV uClinux PIC relocs to weak undefined symbols, pie linking
#source: fdpic6.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -pie --defsym WD1=D6

.*:     file format elf.*frv.*

Disassembly of section \.plt:

[0-9a-f ]+<\.plt>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f ]+:	c0 1a 00 06 	bra [0-9a-f]+ <F6-0x10>
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	c0 1a 00 04 	bra [0-9a-f]+ <F6-0x10>
[0-9a-f ]+:	00 00 00 10 	add\.p gr0,gr16,gr0
[0-9a-f ]+:	c0 1a 00 02 	bra [0-9a-f]+ <F6-0x10>
[0-9a-f ]+:	00 00 00 18 	add\.p gr0,gr24,gr0
[0-9a-f ]+:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
[0-9a-f ]+:	80 30 40 00 	jmpl @\(gr4,gr0\)
[0-9a-f ]+:	9c cc ff f0 	lddi @\(gr15,-16\),gr14
[0-9a-f ]+:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

[0-9a-f ]+<F6>:
[0-9a-f ]+:	fe 3f ff fe 	call [0-9a-f]+ <F6-0x8>
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 24 	setlos 0x24,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 18 	setlos 0x18,gr0
[0-9a-f ]+:	80 f4 00 1c 	setlo 0x1c,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff e8 	setlos 0xf*ffffffe8,gr0
[0-9a-f ]+:	80 f4 ff e0 	setlo 0xffe0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 ff d0 	setlo 0xffd0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D6>:
	\.\.\.
[0-9a-f	 ]+: R_FRV_32	WD0
[0-9a-f	 ]+: R_FRV_FUNCDESC	WFb
[0-9a-f	 ]+: R_FRV_32	WFb
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x20>:
[0-9a-f ]+:	00 00 04 b8 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	WF9
[0-9a-f ]+:	00 00 00 02 	.*
[0-9a-f ]+:	00 00 04 b0 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	WF8
[0-9a-f ]+:	00 00 00 02 	.*
[0-9a-f ]+:	00 00 04 a8 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	WF0
[0-9a-f ]+:	00 00 00 02 	.*
[0-9a-f ]+:	00 00 04 a0 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	WF7
[0-9a-f ]+:	00 00 00 02 	.*

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f	 ]+: R_FRV_32	WF1
[0-9a-f	 ]+: R_FRV_FUNCDESC	WF4
[0-9a-f	 ]+: R_FRV_32	WD2
[0-9a-f	 ]+: R_FRV_FUNCDESC	WF5
[0-9a-f	 ]+: R_FRV_FUNCDESC	WF6
[0-9a-f	 ]+: R_FRV_32	WF3
[0-9a-f	 ]+: R_FRV_32	WF2
