#name: FRV uClinux PIC relocs to forced-local symbols with addends, shared linking
#source: fdpic8.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -shared --version-script fdpic8.ldv

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
[0-9a-f ]+:	80 f4 ff c8 	setlo 0xffc8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff c4 	addi gr15,-60,gr0
[0-9a-f ]+:	80 fc ff c4 	setlos 0xf+fc4,gr0
[0-9a-f ]+:	80 f4 ff c4 	setlo 0xffc4,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 20 	setlo 0x20,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D8>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.data

[0-9a-f ]+<GD0>:
[0-9a-f ]+:	00 00 00 10 	add\.p gr0,gr16,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x38>:
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
[0-9a-f ]+:	00 00 00 20 	add\.p gr0,gr32,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 18 	add\.p gr0,gr24,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.data
[0-9a-f ]+:	00 00 00 08 	add\.p gr0,gr8,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
