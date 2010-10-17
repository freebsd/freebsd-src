#objdump: -dr -z -mmips:4000
#as: -mips3 -mtune=r4000 -mips16
#name: mips jalx
.*:     file format .*
Disassembly of section .text:
00000000 <.text>:
   0:	74000000 	jalx	0x0
			0: R_MIPS_26	external_label
   4:	00000000 	nop
   8:	00000000 	nop
   c:	00000000 	nop
