#name: FRV uClinux PIC relocs to local symbols, shared linking
#source: fdpic1.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#ld: -shared

.*:     file format elf.*frv.*

Disassembly of section \.text:

[0-9a-f ]+<F1>:
[0-9a-f ]+:	80 3c 00 01 	call [0-9a-f]+ <\.F0>

[0-9a-f ]+<\.F0>:
[0-9a-f ]+:	80 40 f0 0c 	addi gr15,12,gr0
[0-9a-f ]+:	80 fc 00 0c 	setlos 0xc,gr0
[0-9a-f ]+:	80 f4 00 0c 	setlo 0xc,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 f0 10 	addi gr15,16,gr0
[0-9a-f ]+:	80 fc 00 10 	setlos 0x10,gr0
[0-9a-f ]+:	80 f4 00 10 	setlo 0x10,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
[0-9a-f ]+:	80 40 ff f8 	addi gr15,-8,gr0
[0-9a-f ]+:	80 fc ff f8 	setlos 0xf+ff8,gr0
[0-9a-f ]+:	80 f4 ff f8 	setlo 0xfff8,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 40 ff ec 	addi gr15,-20,gr0
[0-9a-f ]+:	80 fc ff ec 	setlos 0xf+fec,gr0
[0-9a-f ]+:	80 f4 ff ec 	setlo 0xffec,gr0
[0-9a-f ]+:	80 f8 ff ff 	sethi 0xffff,gr0
[0-9a-f ]+:	80 f4 00 14 	setlo 0x14,gr0
[0-9a-f ]+:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.dat[0-9a-f ]+:

[0-9a-f ]+<D1>:
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_32	\.data

[0-9a-f ]+<\.D0>:
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
Disassembly of section \.got:

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_-0x8>:
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_FUNCDESC_VALUE	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0

[0-9a-f ]+<_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_32	\.text
[0-9a-f ]+:	00 00 00 00 	add\.p gr0,gr0,gr0
[0-9a-f	 ]+: R_FRV_32	\.got
[0-9a-f ]+:	00 00 00 04 	add\.p gr0,gr4,gr0
[0-9a-f	 ]+: R_FRV_32	\.data
