#name: FRV TLS undefweak relocs, static linking with relaxation
#source: tls-3.s
#objdump: -D -j .text -j .got -j .plt
#ld: -static --relax

.*:     file format elf.*frv.*

Disassembly of section \.text:

00010094 <_start>:
   10094:	92 fc 00 00 	setlos lo\(0x0\),gr9
   10098:	00 88 00 00 	nop\.p
   1009c:	80 88 00 00 	nop
   100a0:	92 fc 00 00 	setlos lo\(0x0\),gr9
   100a4:	80 88 00 00 	nop
   100a8:	12 fc 00 00 	setlos\.p lo\(0x0\),gr9
   100ac:	80 88 00 00 	nop
   100b0:	80 88 00 00 	nop
   100b4:	92 fc 00 00 	setlos lo\(0x0\),gr9
   100b8:	00 88 00 00 	nop\.p
   100bc:	80 88 00 00 	nop
   100c0:	92 fc 00 00 	setlos lo\(0x0\),gr9
Disassembly of section \.got:

000140c8 <(__data_start|_GLOBAL_OFFSET_TABLE_)>:
	\.\.\.
