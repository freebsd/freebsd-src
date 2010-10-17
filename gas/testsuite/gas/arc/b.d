#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <text_label>:
   0:	80 ff ff 27 	27ffff80     b          0 <text_label>

   4:	00 ff ff 27 	27ffff00     b          0 <text_label>

   8:	80 fe ff 27 	27fffe80     b          0 <text_label>

   c:	01 fe ff 27 	27fffe01     bz         0 <text_label>

  10:	81 fd ff 27 	27fffd81     bz         0 <text_label>

  14:	02 fd ff 27 	27fffd02     bnz        0 <text_label>

  18:	82 fc ff 27 	27fffc82     bnz        0 <text_label>

  1c:	03 fc ff 27 	27fffc03     bp         0 <text_label>

  20:	83 fb ff 27 	27fffb83     bp         0 <text_label>

  24:	04 fb ff 27 	27fffb04     bn         0 <text_label>

  28:	84 fa ff 27 	27fffa84     bn         0 <text_label>

  2c:	05 fa ff 27 	27fffa05     bc         0 <text_label>

  30:	85 f9 ff 27 	27fff985     bc         0 <text_label>

  34:	05 f9 ff 27 	27fff905     bc         0 <text_label>

  38:	86 f8 ff 27 	27fff886     bnc        0 <text_label>

  3c:	06 f8 ff 27 	27fff806     bnc        0 <text_label>

  40:	86 f7 ff 27 	27fff786     bnc        0 <text_label>

  44:	07 f7 ff 27 	27fff707     bv         0 <text_label>

  48:	87 f6 ff 27 	27fff687     bv         0 <text_label>

  4c:	08 f6 ff 27 	27fff608     bnv        0 <text_label>

  50:	88 f5 ff 27 	27fff588     bnv        0 <text_label>

  54:	09 f5 ff 27 	27fff509     bgt        0 <text_label>

  58:	8a f4 ff 27 	27fff48a     bge        0 <text_label>

  5c:	0b f4 ff 27 	27fff40b     blt        0 <text_label>

  60:	8c f3 ff 27 	27fff38c     ble        0 <text_label>

  64:	0d f3 ff 27 	27fff30d     bhi        0 <text_label>

  68:	8e f2 ff 27 	27fff28e     bls        0 <text_label>

  6c:	0f f2 ff 27 	27fff20f     bpnz       0 <text_label>

  70:	a0 f1 ff 27 	27fff1a0     b.d        0 <text_label>

  74:	00 f1 ff 27 	27fff100     b          0 <text_label>

  78:	c0 f0 ff 27 	27fff0c0     b.jd       0 <text_label>

  7c:	21 f0 ff 27 	27fff021     bz.d       0 <text_label>

  80:	82 ef ff 27 	27ffef82     bnz        0 <text_label>

  84:	46 ef ff 27 	27ffef46     bnc.jd     0 <text_label>

