#name: FRV uClinux PIC relocs to local symbols with addends, shared linking
#source: fdpic7.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -shared

.*:     file format elf.*frv.*

Disassembly of section \.text:

000003dc <F7>:
 3dc:	80 3c 00 02 	call 3e4 <\.F0\+0x4>

000003e0 <\.F0>:
 3e0:	80 40 f0 0c 	addi gr15,12,gr0
 3e4:	80 fc 00 0c 	setlos 0xc,gr0
 3e8:	80 f4 00 0c 	setlo 0xc,gr0
 3ec:	80 f8 00 00 	sethi hi\(0x0\),gr0
 3f0:	80 40 f0 10 	addi gr15,16,gr0
 3f4:	80 fc 00 10 	setlos 0x10,gr0
 3f8:	80 f4 00 10 	setlo 0x10,gr0
 3fc:	80 f8 00 00 	sethi hi\(0x0\),gr0
 400:	80 40 ff f8 	addi gr15,-8,gr0
 404:	80 fc ff f8 	setlos 0xfffffff8,gr0
 408:	80 f4 ff f8 	setlo 0xfff8,gr0
 40c:	80 f8 ff ff 	sethi 0xffff,gr0
 410:	80 40 ff 7c 	addi gr15,-132,gr0
 414:	80 fc ff 7c 	setlos 0xffffff7c,gr0
 418:	80 f4 ff 7c 	setlo 0xff7c,gr0
 41c:	80 f8 ff ff 	sethi 0xffff,gr0
 420:	80 f4 00 14 	setlo 0x14,gr0
 424:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

0000442c <D7>:
    442c:	00 00 00 08 	add\.p gr0,gr8,gr0
			442c: R_FRV_32	\.data

00004430 <\.D0>:
    4430:	00 00 00 00 	add\.p gr0,gr0,gr0
			4430: R_FRV_32	\.got
    4434:	00 00 00 08 	add\.p gr0,gr8,gr0
			4434: R_FRV_32	\.text
Disassembly of section \.got:

000044b0 <_GLOBAL_OFFSET_TABLE_-0x8>:
    44b0:	00 00 00 08 	add\.p gr0,gr8,gr0
			44b0: R_FRV_FUNCDESC_VALUE	\.text
    44b4:	00 00 00 00 	add\.p gr0,gr0,gr0

000044b8 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
    44c4:	00 00 00 08 	add\.p gr0,gr8,gr0
			44c4: R_FRV_32	\.text
    44c8:	00 00 00 00 	add\.p gr0,gr0,gr0
			44c8: R_FRV_32	\.got
    44cc:	00 00 00 08 	add\.p gr0,gr8,gr0
			44cc: R_FRV_32	\.data
