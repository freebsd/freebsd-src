#name: FRV uClinux PIC relocs to forced-local symbols, shared linking
#source: fdpic2.s
#objdump: -DR -j .text -j .data -j .got -j .plt
#as: -mfdpic
#ld: -shared --version-script fdpic2.ldv

.*:     file format elf.*frv.*

Disassembly of section \.text:

00000300 <F2>:
 300:	80 3c 00 01 	call 304 <GF0>

00000304 <GF0>:
 304:	80 40 f0 10 	addi gr15,16,gr0
 308:	80 fc 00 24 	setlos 0x24,gr0
 30c:	80 f4 00 20 	setlo 0x20,gr0
 310:	80 f8 00 00 	sethi hi\(0x0\),gr0
 314:	80 40 f0 0c 	addi gr15,12,gr0
 318:	80 fc 00 18 	setlos 0x18,gr0
 31c:	80 f4 00 14 	setlo 0x14,gr0
 320:	80 f8 00 00 	sethi hi\(0x0\),gr0
 324:	80 40 ff f8 	addi gr15,-8,gr0
 328:	80 fc ff d0 	setlos 0xffffffd0,gr0
 32c:	80 f4 ff c8 	setlo 0xffc8,gr0
 330:	80 f8 ff ff 	sethi 0xffff,gr0
 334:	80 40 ff 44 	addi gr15,-188,gr0
 338:	80 fc ff 44 	setlos 0xffffff44,gr0
 33c:	80 f4 ff 44 	setlo 0xff44,gr0
 340:	80 f8 ff ff 	sethi 0xffff,gr0
 344:	80 f4 00 1c 	setlo 0x1c,gr0
 348:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.data:

00004350 <D2>:
    4350:	00 00 00 04 	add\.p gr0,gr4,gr0
			4350: R_FRV_32	\.data

00004354 <GD0>:
    4354:	00 00 00 18 	add\.p gr0,gr24,gr0
			4354: R_FRV_32	\.got
    4358:	00 00 00 04 	add\.p gr0,gr4,gr0
			4358: R_FRV_32	\.text
Disassembly of section \.got:

000043d8 <_GLOBAL_OFFSET_TABLE_-0x38>:
    43d8:	00 00 00 04 	add\.p gr0,gr4,gr0
			43d8: R_FRV_FUNCDESC_VALUE	\.text
    43dc:	00 00 00 00 	add\.p gr0,gr0,gr0
    43e0:	00 00 00 04 	add\.p gr0,gr4,gr0
			43e0: R_FRV_FUNCDESC_VALUE	\.text
    43e4:	00 00 00 00 	add\.p gr0,gr0,gr0
    43e8:	00 00 00 04 	add\.p gr0,gr4,gr0
			43e8: R_FRV_FUNCDESC_VALUE	\.text
    43ec:	00 00 00 00 	add\.p gr0,gr0,gr0
    43f0:	00 00 00 04 	add\.p gr0,gr4,gr0
			43f0: R_FRV_FUNCDESC_VALUE	\.text
    43f4:	00 00 00 00 	add\.p gr0,gr0,gr0
    43f8:	00 00 00 04 	add\.p gr0,gr4,gr0
			43f8: R_FRV_FUNCDESC_VALUE	\.text
    43fc:	00 00 00 00 	add\.p gr0,gr0,gr0
    4400:	00 00 00 04 	add\.p gr0,gr4,gr0
			4400: R_FRV_FUNCDESC_VALUE	\.text
    4404:	00 00 00 00 	add\.p gr0,gr0,gr0
    4408:	00 00 00 04 	add\.p gr0,gr4,gr0
			4408: R_FRV_FUNCDESC_VALUE	\.text
    440c:	00 00 00 00 	add\.p gr0,gr0,gr0

00004410 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
    441c:	00 00 00 10 	add\.p gr0,gr16,gr0
			441c: R_FRV_32	\.got
    4420:	00 00 00 04 	add\.p gr0,gr4,gr0
			4420: R_FRV_32	\.text
    4424:	00 00 00 28 	add\.p gr0,gr40,gr0
			4424: R_FRV_32	\.got
    4428:	00 00 00 20 	add\.p gr0,gr32,gr0
			4428: R_FRV_32	\.got
    442c:	00 00 00 04 	add\.p gr0,gr4,gr0
			442c: R_FRV_32	\.data
    4430:	00 00 00 04 	add\.p gr0,gr4,gr0
			4430: R_FRV_32	\.text
    4434:	00 00 00 04 	add\.p gr0,gr4,gr0
			4434: R_FRV_32	\.text
