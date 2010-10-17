#name: i386 jump
#objdump: -drw

.*: +file format .*i386.*

Disassembly of section .text:

0+000 <.text>:
   0:	eb fe [ 	]*jmp    (0x0|0 <.text>)
   2:	e9 ((fc|f9) ff ff ff|00 00 00 00) [ 	]*jmp    (0x)?(0|3|7)( <.text(\+0x7)?>)?	3: (R_386_PC)?(DISP)?32	xxx
   7:	ff 25 00 00 00 00 [ 	]*jmp    \*0x0	9: (R_386_)?(dir)?32	xxx
   d:	ff e7 [ 	]*jmp    \*%edi
   f:	ff 27 [ 	]*jmp    \*\(%edi\)
  11:	ff 2c bd 00 00 00 00 [ 	]*ljmp   \*0x0\(,%edi,4\)	14: (R_386_)?(dir)?32	xxx
  18:	ff 2d 00 00 00 00 [ 	]*ljmp   \*0x0	1a: (R_386_)?(dir)?32	xxx
  1e:	ea 00 00 00 00 34 12 [ 	]*ljmp   \$0x1234,\$0x0	1f: (R_386_)?(dir)?32	xxx
  25:	e8 d6 ff ff ff [ 	]*call   (0x0|0 <.text>)
  2a:	e8 ((fc|d1) ff ff ff|00 00 00 00) [ 	]*call   (0x)?(0|2b|2f)( <.text(\+0x2f)?>)?	2b: (R_386_PC)?(DISP)?32	xxx
  2f:	ff 15 00 00 00 00 [ 	]*call   \*0x0	31: (R_386_)?(dir)?32	xxx
  35:	ff d7 [ 	]*call   \*%edi
  37:	ff 17 [ 	]*call   \*\(%edi\)
  39:	ff 1c bd 00 00 00 00 [ 	]*lcall  \*0x0\(,%edi,4\)	3c: (R_386_)?(dir)?32	xxx
  40:	ff 1d 00 00 00 00 [ 	]*lcall  \*0x0	42: (R_386_)?(dir)?32	xxx
  46:	9a 00 00 00 00 34 12 [ 	]*lcall  \$0x1234,\$0x0	47: (R_386_)?(dir)?32	xxx
  4d:.*
.*
