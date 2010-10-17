#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <text_label>:
   0:	80 ff ff 37 	37ffff80     lp         0 <text_label>

   4:	00 ff ff 37 	37ffff00     lp         0 <text_label>

   8:	80 fe ff 37 	37fffe80     lp         0 <text_label>

   c:	01 fe ff 37 	37fffe01     lpz        0 <text_label>

  10:	81 fd ff 37 	37fffd81     lpz        0 <text_label>

  14:	02 fd ff 37 	37fffd02     lpnz       0 <text_label>

  18:	82 fc ff 37 	37fffc82     lpnz       0 <text_label>

  1c:	03 fc ff 37 	37fffc03     lpp        0 <text_label>

  20:	83 fb ff 37 	37fffb83     lpp        0 <text_label>

  24:	04 fb ff 37 	37fffb04     lpn        0 <text_label>

  28:	84 fa ff 37 	37fffa84     lpn        0 <text_label>

  2c:	05 fa ff 37 	37fffa05     lpc        0 <text_label>

  30:	85 f9 ff 37 	37fff985     lpc        0 <text_label>

  34:	05 f9 ff 37 	37fff905     lpc        0 <text_label>

  38:	86 f8 ff 37 	37fff886     lpnc       0 <text_label>

  3c:	06 f8 ff 37 	37fff806     lpnc       0 <text_label>

  40:	86 f7 ff 37 	37fff786     lpnc       0 <text_label>

  44:	07 f7 ff 37 	37fff707     lpv        0 <text_label>

  48:	87 f6 ff 37 	37fff687     lpv        0 <text_label>

  4c:	08 f6 ff 37 	37fff608     lpnv       0 <text_label>

  50:	88 f5 ff 37 	37fff588     lpnv       0 <text_label>

  54:	09 f5 ff 37 	37fff509     lpgt       0 <text_label>

  58:	8a f4 ff 37 	37fff48a     lpge       0 <text_label>

  5c:	0b f4 ff 37 	37fff40b     lplt       0 <text_label>

  60:	8c f3 ff 37 	37fff38c     lple       0 <text_label>

  64:	0d f3 ff 37 	37fff30d     lphi       0 <text_label>

  68:	8e f2 ff 37 	37fff28e     lpls       0 <text_label>

  6c:	0f f2 ff 37 	37fff20f     lppnz      0 <text_label>

  70:	a0 f1 ff 37 	37fff1a0     lp.d       0 <text_label>

  74:	00 f1 ff 37 	37fff100     lp         0 <text_label>

  78:	c0 f0 ff 37 	37fff0c0     lp.jd      0 <text_label>

  7c:	21 f0 ff 37 	37fff021     lpz.d      0 <text_label>

  80:	82 ef ff 37 	37ffef82     lpnz       0 <text_label>

  84:	46 ef ff 37 	37ffef46     lpnc.jd    0 <text_label>

