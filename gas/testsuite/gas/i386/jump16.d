#name: i386 jump16
#objdump: -drw -mi8086

.*:     file format .*i386.*

Disassembly of section .text:

0+000 <.text>:
   0:	eb fe [ 	]*jmp    (0x0|0 <.text>)
   2:	e9 (fe|fb) ff [ 	]*jmp    (0x3|0x0|0 <.text>)	3: (R_386_PC)?(DISP)?16	xxx
   5:	ff 26 00 00 [ 	]*jmp    \*0	7: (R_386_)?(dir)?16	xxx
   9:	66 ff e7 [ 	]*jmpl   \*%edi
   c:	67 ff 27 [ 	]*addr32 jmp \*\(%edi\)
   f:	67 ff af 00 00 00 00 [ 	]*addr32 ljmp \*0x0\(%edi\)	12: (R_386_)?(dir)?32	xxx
  16:	ff 2e 00 00 [ 	]*ljmp   \*0	18: (R_386_)?(dir)?16	xxx
  1a:	ea 00 00 34 12 [ 	]*ljmp   \$0x1234,\$0x0	1b: (R_386_)?(dir)?16	xxx
  1f:	66 e8 db ff ff ff [ 	]*calll  (0x0|0 <.text>)
  25:	66 e8 (fc|d5) ff ff ff [ 	]*calll  (0x27|0x0|0 <.text>)	27: (R_386_PC)?(DISP)?32	xxx
  2b:	66 ff 16 00 00 [ 	]*calll  \*0	2e: (R_386_)?(dir)?16	xxx
  30:	66 ff d7 [ 	]*calll  \*%edi
  33:	67 66 ff 17 [ 	]*addr32 calll \*\(%edi\)
  37:	67 66 ff 9f 00 00 00 00 [ 	]*addr32 lcalll \*0x0\(%edi\)	3b: (R_386_)?(dir)?32	xxx
  3f:	66 ff 1e 00 00 [ 	]*lcalll \*0	42: (R_386_)?(dir)?16	xxx
  44:	66 9a 00 00 00 00 34 12 [ 	]*lcalll \$0x1234,\$0x0	46: (R_386_)?(dir)?32	xxx
  4c:	eb b2 [ 	]*jmp    (0x0|0 <.text>)
  4e:	ff 26 00 00 [ 	]*jmp    \*0	50: (R_386_)?(dir)?16	xxx
  52:	ff e7 [ 	]*jmp    \*%di
  54:	ff 25 [ 	]*jmp    \*\(%di\)
  56:	ff ad 00 00 [ 	]*ljmp   \*0\(%di\)	58: (R_386_)?(dir)?16	xxx
  5a:	ff 2e 00 00 [ 	]*ljmp   \*0	5c: (R_386_)?(dir)?16	xxx
  5e:	ea 00 00 34 12 [ 	]*ljmp   \$0x1234,\$0x0	5f: (R_386_)?(dir)?16	xxx
  63:	e8 9a ff [ 	]*call   (0x0|0 <.text>)
  66:	e8 (fe|97) ff [ 	]*call   (0x67|0x0|0 <.text>)	67: (R_386_PC)?(DISP)?16	xxx
  69:	ff 16 00 00 [ 	]*call   \*0	6b: (R_386_)?(dir)?16	xxx
  6d:	ff d7 [ 	]*call   \*%di
  6f:	ff 15 [ 	]*call   \*\(%di\)
  71:	ff 9d 00 00 [ 	]*lcall  \*0\(%di\)	73: (R_386_)?(dir)?16	xxx
  75:	ff 1e 00 00 [ 	]*lcall  \*0	77: (R_386_)?(dir)?16	xxx
  79:	9a 00 00 34 12 [ 	]*lcall  \$0x1234,\$0x0	7a: (R_386_)?(dir)?16	xxx
	...
