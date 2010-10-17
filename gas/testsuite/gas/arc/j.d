#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <text_label>:
   0:	00 00 1f 38 	381f0000     j          0 <text_label>

   4:	00 00 00 00 
			4: R_ARC_B26	.text
   8:	00 00 1f 38 	381f0000     j          0 <text_label>

   c:	00 00 00 00 
			c: R_ARC_B26	.text
  10:	00 00 1f 38 	381f0000     j          0 <text_label>

  14:	00 00 00 00 
			14: R_ARC_B26	.text
  18:	01 00 1f 38 	381f0001     jz         0 <text_label>

  1c:	00 00 00 00 
			1c: R_ARC_B26	.text
  20:	01 00 1f 38 	381f0001     jz         0 <text_label>

  24:	00 00 00 00 
			24: R_ARC_B26	.text
  28:	02 00 1f 38 	381f0002     jnz        0 <text_label>

  2c:	00 00 00 00 
			2c: R_ARC_B26	.text
  30:	02 00 1f 38 	381f0002     jnz        0 <text_label>

  34:	00 00 00 00 
			34: R_ARC_B26	.text
  38:	03 00 1f 38 	381f0003     jp         0 <text_label>

  3c:	00 00 00 00 
			3c: R_ARC_B26	.text
  40:	03 00 1f 38 	381f0003     jp         0 <text_label>

  44:	00 00 00 00 
			44: R_ARC_B26	.text
  48:	04 00 1f 38 	381f0004     jn         0 <text_label>

  4c:	00 00 00 00 
			4c: R_ARC_B26	.text
  50:	04 00 1f 38 	381f0004     jn         0 <text_label>

  54:	00 00 00 00 
			54: R_ARC_B26	.text
  58:	05 00 1f 38 	381f0005     jc         0 <text_label>

  5c:	00 00 00 00 
			5c: R_ARC_B26	.text
  60:	05 00 1f 38 	381f0005     jc         0 <text_label>

  64:	00 00 00 00 
			64: R_ARC_B26	.text
  68:	05 00 1f 38 	381f0005     jc         0 <text_label>

  6c:	00 00 00 00 
			6c: R_ARC_B26	.text
  70:	06 00 1f 38 	381f0006     jnc        0 <text_label>

  74:	00 00 00 00 
			74: R_ARC_B26	.text
  78:	06 00 1f 38 	381f0006     jnc        0 <text_label>

  7c:	00 00 00 00 
			7c: R_ARC_B26	.text
  80:	06 00 1f 38 	381f0006     jnc        0 <text_label>

  84:	00 00 00 00 
			84: R_ARC_B26	.text
  88:	07 00 1f 38 	381f0007     jv         0 <text_label>

  8c:	00 00 00 00 
			8c: R_ARC_B26	.text
  90:	07 00 1f 38 	381f0007     jv         0 <text_label>

  94:	00 00 00 00 
			94: R_ARC_B26	.text
  98:	08 00 1f 38 	381f0008     jnv        0 <text_label>

  9c:	00 00 00 00 
			9c: R_ARC_B26	.text
  a0:	08 00 1f 38 	381f0008     jnv        0 <text_label>

  a4:	00 00 00 00 
			a4: R_ARC_B26	.text
  a8:	09 00 1f 38 	381f0009     jgt        0 <text_label>

  ac:	00 00 00 00 
			ac: R_ARC_B26	.text
  b0:	0a 00 1f 38 	381f000a     jge        0 <text_label>

  b4:	00 00 00 00 
			b4: R_ARC_B26	.text
  b8:	0b 00 1f 38 	381f000b     jlt        0 <text_label>

  bc:	00 00 00 00 
			bc: R_ARC_B26	.text
  c0:	0c 00 1f 38 	381f000c     jle        0 <text_label>

  c4:	00 00 00 00 
			c4: R_ARC_B26	.text
  c8:	0d 00 1f 38 	381f000d     jhi        0 <text_label>

  cc:	00 00 00 00 
			cc: R_ARC_B26	.text
  d0:	0e 00 1f 38 	381f000e     jls        0 <text_label>

  d4:	00 00 00 00 
			d4: R_ARC_B26	.text
  d8:	0f 00 1f 38 	381f000f     jpnz       0 <text_label>

  dc:	00 00 00 00 
			dc: R_ARC_B26	.text
  e0:	00 00 1f 38 	381f0000     j          0 <text_label>

  e4:	00 00 00 00 
			e4: R_ARC_B26	external_text_label
  e8:	00 00 1f 38 	381f0000     j          0 <text_label>

  ec:	00 00 00 00 
