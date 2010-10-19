#name: FRV uClinux PIC relocs to (mostly) global symbols, shared linking
#source: fdpic2.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -shared --version-script fdpic2min.ldv

.*:     file format elf.*frv.*

Disassembly of section \.plt:

[0-9a-f ]+ <\.plt>:
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	c0 1a 00 06 	bra [0-9a-f]+ <F2-0x10>
[0-9a-f ]+:	00 00 00 10 	add\.p gr0,gr16,gr0
[0-9a-f ]+:	c0 1a 00 04 	bra [0-9a-f]+ <F2-0x10>
[0-9a-f ]+:	00 00 00 18 	add\.p gr0,gr24,gr0
[0-9a-f ]+:	c0 1a 00 02 	bra [0-9a-f]+ <F2-0x10>
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f ]+:	88 08 f1 40 	ldd @\(gr15,gr0\),gr4
[0-9a-f ]+:	80 30 40 00 	jmpl @\(gr4,gr0\)
[0-9a-f ]+:	9c cc ff f8 	lddi @\(gr15,-8\),gr14
[0-9a-f ]+:	80 30 e0 00 	jmpl @\(gr14,gr0\)
Disassembly of section \.text:

[0-9a-f ]+<F2>:
[0-9a-f ]+:	fe 3f ff fe 	call [0-9a-f]+ <F2-0x8>

[0-9a-f ]+<GF0>:
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 24 	setlos 0x24,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 18 	setlos 0x18,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f0 	addi gr15,-16,gr0
[0-9a-f ]+:	80 fc ff e8 	setlos 0xf*ffffffe8,gr0
[0-9a-f ]+:	80 f4 ff e0 	setlo 0xffe0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff d8 	addi gr15,-40,gr0
[0-9a-f ]+:	80 fc ff d8 	setlos 0xf+fd8,gr0
[0-9a-f ]+:	80 f4 ff d8 	setlo 0xffd8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 1c 	setlo 0x1c,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D2>:
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_32	GD0

[0-9a-f ]+<GD0>:
	\.\.\.
[0-9a-f	 ]+: R_FRV_FUNCDESC	GFb
[0-9a-f	 ]+: R_FRV_32	GFb
[0-9A-F ]+isassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x20>:
[0-9a-f ]+:	00 00 04 a4 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	GF9
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 9c 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	GF8
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 ac 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	GF7
[0-9a-f ]+:	00 00 00 00 	.*
[0-9a-f ]+:	00 00 04 94 	.*
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	GF0
[0-9a-f ]+:	00 00 00 00 	.*

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f	 ]+: R_FRV_FUNCDESC	GF4
[0-9a-f	 ]+: R_FRV_32	GF1
[0-9a-f	 ]+: R_FRV_FUNCDESC	GF6
[0-9a-f	 ]+: R_FRV_FUNCDESC	GF5
[0-9a-f	 ]+: R_FRV_32	GD4
[0-9a-f	 ]+: R_FRV_32	GF3
[0-9a-f	 ]+: R_FRV_32	GF2
