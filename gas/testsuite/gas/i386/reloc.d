#objdump: -drw
#name: i386 reloc

.*: +file format .*i386.*

Disassembly of section .text:

0+000 <foo>:
   0:	b3 00 [ 	]*mov    \$0x0,%bl	1: (R_386_)?8	.text
   2:	68 00 00 00 00 [ 	]*push   \$0x0	3: (R_386_)?(dir)?32	.text
   7:	05 00 00 00 00 [ 	]*add    \$0x0,%eax	8: (R_386_)?(dir)?32	.text
   c:	81 c3 00 00 00 00 [ 	]*add    \$0x0,%ebx	e: (R_386_)?(dir)?32	.text
  12:	69 d2 00 00 00 00 [ 	]*imul   \$0x0,%edx,%edx	14: (R_386_)?(dir)?32	.text
  18:	9a 00 00 00 00 00 00 [ 	]*lcall  \$0x0,\$0x0	19: (R_386_)?(dir)?32	.text
  1f:	66 68 00 00 [ 	]*pushw  \$0x0	21: (R_386_)?16	.text
  23:	90 [ 	]*nop    
  24:	90 [ 	]*nop    
  25:	90 [ 	]*nop    
  26:	90 [ 	]*nop    
  27:	90 [ 	]*nop    
