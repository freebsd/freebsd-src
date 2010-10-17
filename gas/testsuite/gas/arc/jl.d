#as: -EL -marc6
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <text_label>:
   0:	40 02 1f 38 	381f0240     jl         0 <text_label>

   4:	00 00 00 00 
			4: R_ARC_B26	.text
   8:	40 03 1f 38 	381f0340     jl.f       0 <text_label>

   c:	00 00 00 00 
			c: R_ARC_B26	.text
  10:	02 82 00 38 	38008202     jlnz       \[r1\]
  14:	40 02 1f 38 	381f0240     jl         0 <text_label>

  18:	00 00 00 00 
			18: R_ARC_B26	.text
  1c:	40 03 1f 38 	381f0340     jl.f       0 <text_label>

  20:	00 00 00 00 
			20: R_ARC_B26	.text
