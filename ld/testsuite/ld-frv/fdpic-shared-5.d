#name: FRV uClinux PIC relocs to undefined symbols, shared linking
#source: fdpic5.s
#objdump: -DRz -j .text -j .data -j .got -j .plt
#ld: -shared

.*:     file format elf.*frv.*

Disassembly of section \.plt:

[0-9a-f ]+<\.plt>:
[0-9a-f ]+:	00 00 00 10 	add\.p gr0,gr16,gr0
[0-9a-f ]+:	c0 1a 00 06 	bra [0-9a-f]+ <F5-0x10>
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f ]+:	c0 1a 00 04 	bra [0-9a-f]+ <F5-0x10>
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	c0 1a 00 02 	bra [0-9a-f]+ <F5-0x10>
[0-9a-f ]+:	00 00 00 18 	add\.p gr0,gr24,gr0
[0-9a-f ]+:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
[0-9a-f ]+:	80 30 40 00 	jmpl @\(gr4,gr0\)
[0-9a-f ]+:	9c cc ff f0 	lddi @\(gr15,-16\),gr14
[0-9a-f ]+:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

[0-9a-f ]+<F5>:
[0-9a-f ]+:	fe 3f ff fe 	call [0-9a-f]+ <F5-0x8>
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 24 	setlos 0x24,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 1c 	setlos 0x1c,gr0
[0-9a-f ]+:	80 f4 00 18 	setlo 0x18,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff e8 	setlos 0xf*ffffffe8,gr0
[0-9a-f ]+:	80 f4 ff e0 	setlo 0xffe0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D5>:
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_32	UD0
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC	UFb
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_32	UFb
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x20>:
[0-9a-f ]+:	00 00 04 7c 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	UF9
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 64 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	UF8
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 74 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	UF0
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 6c 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	UF7
[0-9a-f ]+:	00 00 00 00 	.*

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_32	UF1
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC	UF4
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_32	UD1
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC	UF6
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC	UF5
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_32	UF3
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f	 ]+: R_FRV_32	UF2
