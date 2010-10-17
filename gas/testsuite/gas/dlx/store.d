#as:
#objdump: -dr
#name: store

.*: +file format .*

Disassembly of section .text:

00000000 <L1-0x1c>:
   0:	ac 03 00 1c 	sw      0x001c\[r0\],r3
			2: R_DLX_RELOC_16	.text
   4:	ac 03 00 00 	sw      0x0000\[r0\],r3
			6: R_DLX_RELOC_16_HI	.text
   8:	a4 43 ff 90 	sh      0xff90\[r2\],r3
   c:	a0 03 00 3c 	sb      0x003c\[r0\],r3
			e: R_DLX_RELOC_16	.text
  10:	a0 03 00 30 	sb      0x0030\[r0\],r3
  14:	ac 43 00 00 	sw      0x0000\[r2\],r3
  18:	00 00 00 00 	nop

0000001c <L1>:
  1c:	00 00 00 00 	nop
