#name: FRV uClinux PIC relocs to global symbols with addends, pie linking
#source: fdpic8.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -pie

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<F8>:
[0-9a-f ]+:	80 3c 00 02 	call [0-9a-f]+ <GF0\+0x4>

[0-9a-f ]+<GF0>:
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 14 	setlos 0x14,gr0
[0-9a-f ]+:	80 f4 00 24 	setlo 0x24,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 1c 	setlos 0x1c,gr0
[0-9a-f ]+:	80 f4 00 18 	setlo 0x18,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff f0 	setlos 0xf+ff0,gr0
[0-9a-f ]+:	80 f4 ff e8 	setlo 0xffe8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff e0 	addi gr15,-32,gr0
[0-9a-f ]+:	80 fc ff e0 	setlos 0xf+fe0,gr0
[0-9a-f ]+:	80 f4 ff e0 	setlo 0xffe0,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D8>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.data

[0-9a-f ]+<GD0>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC	\.text
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x18>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 02 	add\.p gr0,fp,gr0

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC	\.text
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC	\.text
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC	\.text
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.data
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
