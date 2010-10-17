#name: FRV uClinux PIC relocs to weak undefined symbols, static linking
#source: fdpic6.s
#objdump: -D
#as: -mfdpic
#ld: -static
#error: warn.*different segment

.*:     file format elf.*frv.*

Disassembly of section \.text:

00010000 <F6>:
   10000:	fe 3f c0 00 	call 0 <F6-0x10000>
   10004:	80 40 f0 0c 	addi gr15,12,gr0
   10008:	80 fc 00 24 	setlos 0x24,gr0
   1000c:	80 f4 00 20 	setlo 0x20,gr0
   10010:	80 f8 00 00 	sethi hi\(0x0\),gr0
   10014:	80 40 f0 10 	addi gr15,16,gr0
   10018:	80 fc 00 18 	setlos 0x18,gr0
   1001c:	80 f4 00 1c 	setlo 0x1c,gr0
   10020:	80 f8 00 00 	sethi hi\(0x0\),gr0
   10024:	80 40 ff f8 	addi gr15,-8,gr0
   10028:	80 fc ff f0 	setlos 0xfffffff0,gr0
   1002c:	80 f4 ff e8 	setlo 0xffe8,gr0
   10030:	80 f8 ff ff 	sethi 0xffff,gr0
   10034:	80 f4 ff 18 	setlo 0xff18,gr0
   10038:	80 f8 ff fa 	sethi 0xfffa,gr0
   1003c:	80 f4 00 14 	setlo 0x14,gr0
   10040:	80 f8 00 00 	sethi hi\(0x0\),gr0
Disassembly of section \.rofixup:

00010044 <_gp\+0x800>:
   10044:	00 05 00 f4 	orcc\.p gr16,gr52,gr0,icc0
   10048:	00 05 01 0c 	xor\.p gr16,gr12,gr0
   1004c:	00 05 01 08 	xor\.p gr16,gr8,gr0
   10050:	00 05 00 f8 	orcc\.p gr16,gr56,gr0,icc0
   10054:	00 05 00 c0 	orcc\.p gr16,gr0,gr0,icc0
   10058:	00 05 00 c4 	orcc\.p gr16,gr4,gr0,icc0
   1005c:	00 05 01 00 	xor\.p gr16,gr0,gr0
   10060:	00 05 00 c8 	orcc\.p gr16,gr8,gr0,icc0
   10064:	00 05 00 cc 	orcc\.p gr16,gr12,gr0,icc0
   10068:	00 05 01 04 	xor\.p gr16,gr4,gr0
   1006c:	00 05 00 b8 	or\.p gr16,gr56,gr0
   10070:	00 05 00 bc 	or\.p gr16,gr60,gr0
   10074:	00 05 00 e0 	orcc\.p gr16,gr32,gr0,icc0
   10078:	00 05 00 e4 	orcc\.p gr16,gr36,gr0,icc0
   1007c:	00 05 00 d8 	orcc\.p gr16,gr24,gr0,icc0
   10080:	00 05 00 dc 	orcc\.p gr16,gr28,gr0,icc0
   10084:	00 05 00 d0 	orcc\.p gr16,gr16,gr0,icc0
   10088:	00 05 00 d4 	orcc\.p gr16,gr20,gr0,icc0
   1008c:	00 05 00 fc 	orcc\.p gr16,gr60,gr0,icc0
   10090:	00 05 00 a4 	or\.p gr16,gr36,gr0
   10094:	00 05 00 b0 	or\.p gr16,gr48,gr0
   10098:	00 05 00 b4 	or\.p gr16,gr52,gr0
   1009c:	00 05 00 a8 	or\.p gr16,gr40,gr0
   100a0:	00 05 00 ac 	or\.p gr16,gr44,gr0
Disassembly of section \.data:

000500a4 <D6>:
   500a4:	00 00 00 00 	add\.p gr0,gr0,gr0
   500a8:	00 05 00 b0 	or\.p gr16,gr48,gr0
   500ac:	00 00 00 00 	add\.p gr0,gr0,gr0
Disassembly of section \.got:

000500b0 <_GLOBAL_OFFSET_TABLE_-0x38>:
	\.\.\.

000500e8 <_GLOBAL_OFFSET_TABLE_>:
	\.\.\.
   500f8:	00 05 00 c0 	orcc\.p gr16,gr0,gr0,icc0
   500fc:	00 00 00 00 	add\.p gr0,gr0,gr0
   50100:	00 05 00 c8 	orcc\.p gr16,gr8,gr0,icc0
   50104:	00 05 00 b8 	or\.p gr16,gr56,gr0
	\.\.\.
