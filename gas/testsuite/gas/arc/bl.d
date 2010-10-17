#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <text_label>:
   0:	80 ff ff 2f 	2fffff80     bl         0 <text_label>

   4:	00 ff ff 2f 	2fffff00     bl         0 <text_label>

   8:	80 fe ff 2f 	2ffffe80     bl         0 <text_label>

   c:	01 fe ff 2f 	2ffffe01     blz        0 <text_label>

  10:	81 fd ff 2f 	2ffffd81     blz        0 <text_label>

  14:	02 fd ff 2f 	2ffffd02     blnz       0 <text_label>

  18:	82 fc ff 2f 	2ffffc82     blnz       0 <text_label>

  1c:	03 fc ff 2f 	2ffffc03     blp        0 <text_label>

  20:	83 fb ff 2f 	2ffffb83     blp        0 <text_label>

  24:	04 fb ff 2f 	2ffffb04     bln        0 <text_label>

  28:	84 fa ff 2f 	2ffffa84     bln        0 <text_label>

  2c:	05 fa ff 2f 	2ffffa05     blc        0 <text_label>

  30:	85 f9 ff 2f 	2ffff985     blc        0 <text_label>

  34:	05 f9 ff 2f 	2ffff905     blc        0 <text_label>

  38:	86 f8 ff 2f 	2ffff886     blnc       0 <text_label>

  3c:	06 f8 ff 2f 	2ffff806     blnc       0 <text_label>

  40:	86 f7 ff 2f 	2ffff786     blnc       0 <text_label>

  44:	07 f7 ff 2f 	2ffff707     blv        0 <text_label>

  48:	87 f6 ff 2f 	2ffff687     blv        0 <text_label>

  4c:	08 f6 ff 2f 	2ffff608     blnv       0 <text_label>

  50:	88 f5 ff 2f 	2ffff588     blnv       0 <text_label>

  54:	09 f5 ff 2f 	2ffff509     blgt       0 <text_label>

  58:	8a f4 ff 2f 	2ffff48a     blge       0 <text_label>

  5c:	0b f4 ff 2f 	2ffff40b     bllt       0 <text_label>

  60:	8c f3 ff 2f 	2ffff38c     blle       0 <text_label>

  64:	0d f3 ff 2f 	2ffff30d     blhi       0 <text_label>

  68:	8e f2 ff 2f 	2ffff28e     blls       0 <text_label>

  6c:	0f f2 ff 2f 	2ffff20f     blpnz      0 <text_label>

  70:	a0 f1 ff 2f 	2ffff1a0     bl.d       0 <text_label>

  74:	00 f1 ff 2f 	2ffff100     bl         0 <text_label>

  78:	c0 f0 ff 2f 	2ffff0c0     bl.jd      0 <text_label>

  7c:	21 f0 ff 2f 	2ffff021     blz.d      0 <text_label>

  80:	82 ef ff 2f 	2fffef82     blnz       0 <text_label>

  84:	46 ef ff 2f 	2fffef46     blnc.jd    0 <text_label>

